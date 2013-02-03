/*
* Copyright (C) 1998-2012 The Boeing Company
*                         Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbqueue.c 
*
* Title:    NBQ Listener and Queue Functions [prototype]
*
* Function:
*
*   This file provides routines that implement NodeBrain's NBQ Listener and push
*   through queues.  The NBQ listener is an unauthenticated method of passing
*   information to a NodeBrain server, although file permissions can be used to
*   secure the queue files.
*
* Synopsis:
*
*   #include "nb.h"
*
* Description
*
*   A NodeBrain queue is a directory structure.
*
*     <queue>/<brain>/<identity>/<file>
*
*   Levels of qualification come from multiple places.  When we define a brain
*   we provide the major "queue" name, and the brain name.
*
*     declare <brain> brain (<queue>);  
*
*   When we write to a queue, the active identity provides the next level.
*
*     <queue>/<brain>/<identity>/
*
*   The file name depends on what we are writing to the queue.
*
*     \brain text   ==> ttttttttttt.nnnnnn.q
*
*     copy q        ==> ttttttttttt.nnnnnn.q
*     copy t        ==> ttttttttttt.nnnnnn.t
*     copy c        ==> ttttttttttt.nnnnnn.c
*     copy p        ==> ttttttttttt.nnnnnn.p
*     
*     smtp listener ==> ttttttttttt.nnnnnn.t
*
*   The nbQueueOpenDir() function reads the directory at the brain level to get
*   all identities, and for each identity, reads the directory to get a
*   time ordered list of all files.
*
*      qHandle=nbQueueOpenDir(brainTerm,siName,mode);
*
*   The nbQueueProcess() function calls nbQueueOpenDir() and steps through the
*   list and processes each file.  Seperate functions are provided for
*   processing each type of file. 
*
*      for each queue entry{
*         switch(qEntry->type){
*           case 'q': nbqProcQ(context,filename); break;
*           case 't': nbqProcT(context,filename); break;
*           case 'c': nbqProcC(context,filename);  break;
*           }
*         }
*       
*   The nbQueueRead() function consumes a "q" file.  A "q" format file
*   is marked as we step through it, and may grow while we are reading 
*   it.  Other files must be complete before we start reading them.
*   If they are locked, nbqNext steps over them.  After processsing
*   a queue file, the file is deleted.
*
* Exit Codes:
*
*   0 - Successful completion
*   1 - Error (see message)
* 
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -------------------------------------------------------------------
* 2002/10/05 Ed Trettevik (prototype version introduced in 0.4.1 A6)
* 2002/10/20 eat - version 0.4.1 A8
*             1) Prototyping the nbqDir() and nbqNext() functions.
*                In this first implementation we are not going to order the
*                queue files.  We'll just take them in the order we get them.
*                We'll come back to order them if all goes well.
* 2002/11/13 eat - version 0.4.2 B1
*             1) Trying to work out the kinks in output and input queues.
* 2002/11/30 eat - version 0.4.3 B1
*             1) nbqSend() and nbqSendFile() have been modified to send all
*                queue files in a single session.
* 2002/12/01 eat - version 0.4.3 B1
*             1) include nbQueueCommit() to rename queue files.  This is used to
*                to serialize write and read access to queue files that get
*                unique names.  After the file has been written to a name
*                ignored by queue processors, nbQueueCommit() renames the file
*                making it available to queue processors.  It is up to queue
*                processors to serialize their access to the completed files.
* 2002/12/04 eat - version 0.4.3 B3
*             1) Switching to time interval queue files with size limits and
*                limits on total queue size.
*             2) We have added a queue access control file to a queue to enable
*                serialization on the complete queue directory.
*
*                00000000000.SEND
* 2002/12/07 eat - version 0.4.3 B3
*             1) Test result were not good.  Redesigning the the queue header
*                file to include control data: Time interval file counter for
*                both shared and unique file names.
*
*                00000000000.0000000.Q
*
* 2003/01/13 eat - version 0.4.4 B1
*             1) More work on queue file handling.  nbQueueOpenDir() now 
*                updates the header file to push producers to a new file
*                name.
*
* 2003/03/03 eat 0.5.1  Conditional compile for Max OS X [ see defined(MACOS) ]
*
* 2004/10/06 eat 0.6.1  Conditionals for FreeBSD, OpenBSD, and NetBSD
* 2008-11-11 eat 0.7.3  Changed failure exit code to NB_EXITCODE_FAIL
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-26 dtl 0.8.11 Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=============================================================================
*/
#include <nb/nbi.h>
//#include "nbqueue.h"

//char quedir[100];        /* queue directory  - <brain>.nbq */

/*
*  Lock/Unlock a queue file
*
*    This is used on the queue header file when serializing access
*    to the entire queue.  It is used on an individual queue files
*    to serialize access between multiple producers or consumers.
*
*  int nbQueueLock(file,option,type)
*
*    option   -  0 - unlock, 1 lock if not busy, 2 lock wait
*    type     -  1 consumer, 2 producer
*
*  Returns: -1 - error, 0 - busy, 1 - lock obtained or released
*/
#if defined(WIN32)
int nbQueueLock(HANDLE file,int option,int type){
  OVERLAPPED olap;
  olap.Offset=type;
  olap.OffsetHigh=0;
  olap.hEvent=0;
  switch(option){
    case 0: UnlockFileEx(file,0,1,0,&olap); break;
    case 1: if(!LockFileEx(file,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&olap)) return(0); break;
    case 2: if(!LockFileEx(file,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&olap)) return(-1); break;
    }
  return(1);
  }
#else
int nbQueueLock(int file,int option,int type){
  struct flock lock;

  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET;
  lock.l_start=type;
  lock.l_len=1;
  switch(option){
    case 0: lock.l_type=F_UNLCK; if(fcntl(file,F_SETLKW,&lock)<0) return(-1); break;
    case 1: 
      if(fcntl(file,F_SETLK,&lock)<0){  /* get a lock only if someone else doesn't hold one */
        if(errno==EACCES || errno==EAGAIN) return(0);
        return(-1);
        }
      break;
    case 2: if(fcntl(file,F_SETLKW,&lock)<0) return(-1); break;
    }
  return(1);
  }
#endif

/*
*  Open a queue file by name
*
*  Option and Type parameters are for nbQueueLock(file,option,type)
*
*  Returns: NULL - error or busy, HANDLE - file opened and locked
*/
#if defined(WIN32)
HANDLE *nbQueueOpenFileName(char *filename,int option,int type){
  HANDLE file;
  if((file=CreateFile(filename,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))==NULL){
    outMsg(0,'E',"Unable to open %s",filename);
    return(NBQFILE_ERROR);
    }
  if(nbQueueLock(file,option,type)!=1){
    CloseHandle(file);
    return(NBQFILE_ERROR);
    }
  return(file);
  }
#else
int nbQueueOpenFileName(char *filename,int option,int type){
  int rc;
  int file;
#if defined(mpe) || defined(ANYBSD)
//2012-01-26 dtl: replaced open() with centralised routine openCreate()
//  if((file=open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<=0){
  if((file=openCreate(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<=0){ //dtl: updated
#else
  if((file=openCreate(filename,O_RDWR|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR))<=0){ //dtl:updated
#endif
    outMsg(0,'E',"Unable to open %s",filename);
    return(NBQFILE_ERROR);
    }
  if((rc=nbQueueLock(file,option,type))!=1){
    close(file);
    return(NBQFILE_ERROR);
    }
  return(file);
  }
#endif

#if defined(WIN32)
long nbQueueSeekFile(HANDLE *file,int offset){
  if(offset<0) return(SetFilePointer(file,0,NULL,FILE_END));
  return(SetFilePointer(file,offset,NULL,FILE_BEGIN));
  }
#else
long nbQueueSeekFile(int file,int offset){
  if(offset<0) return(lseek(file,0,SEEK_END));
  return(lseek(file,offset,SEEK_SET));
  }
#endif

#if defined(WIN32)
long nbQueueReadFile(file,buffer,size)
  HANDLE *file;
  char *buffer;
  size_t size; {
  DWORD bytesRead;
  ReadFile(file,buffer,size,&bytesRead,NULL);
  return(bytesRead);
  }
#else
long nbQueueReadFile(int file,char *buffer,size_t size){
  return(read(file,buffer,size)); //size = sizeof(buffer) 
  }
#endif

#if defined(WIN32)
size_t nbQueueWriteFile(HANDLE *file,char *buffer,size_t size){
  DWORD bytesWritten;
  WriteFile(file,buffer,size,&bytesWritten,NULL);
  return(bytesWritten);
  }
#else
size_t nbQueueWriteFile(int file,char *buffer,size_t size){
  return(write(file,buffer,size));
  }
#endif

#if defined(WIN32)
void nbQueueCloseFile(HANDLE *file){
  OVERLAPPED olap;
  olap.OffsetHigh=0;
  olap.hEvent=0;
  olap.Offset=1;
  UnlockFileEx(file,0,2,0,&olap);
  CloseHandle(file);
  }
#else
void nbQueueCloseFile(int file){
  //if(file<3){
  //  outMsg(0,'L',"nbQueueCloseFile() called with fildes=%u",file);
  //  return;
  //  }
  //outMsg(0,'T',"nbQueueCloseFile() closing file");
  close(file); /* UNIX will unlock on close */
  }
#endif

/* 
* Get a queue file name
*
* Option:
*   NBQ_INTERVAL - time interval shared queue file
*   NBQ_NEXT     - next file name
*   NBQ_UNIQUE   - unique queue file (return next but set header to +2)
*
* Type:
*  ' ' - fence           time.count only - Consumers use to set a fence for producers
*   Q - header
*   q - command queue
*   c - command file
*   t - text file
*   p - package
*   
*/
int nbQueueGetFile(char *filename,char *dirname,char *identityName,int qsec,int option,unsigned char type){
  //char dirname[256];   
  //int qsec;  
  NBQFILE hFile;
  struct NBQ_HEADER header;
  size_t bytesRead;
  char newtime[12],newcount[7];
  int x,itime,fcount;  
  int update=1;

  if(trace) outMsg(0,'T',"nbQueueGetFile() called");

  //if(nbqGetDir(dirname,brainTerm)<0) return(-1);
  sprintf(filename,"%s/%s/00000000000.000000.Q",dirname,identityName);
  if(type=='Q') return(0);
  hFile=nbQueueOpenFileName(filename,NBQ_WAIT,NBQ_PRODUCER);
  if(hFile==NBQFILE_ERROR) return(-1);
  bytesRead=nbQueueReadFile(hFile,(char *)&header,sizeof(header));
  if(bytesRead<sizeof(header)){  /* initialize header */
    header.version='3';
    header.comma=',';
    memcpy(header.time,"00000000000",11);
    header.dot='.';
    memcpy(header.count,"000000",6);
    header.nl='\n';
    }
  itime=time(NULL);
  switch(option){
    case NBQ_INTERVAL:  /* shared interval queue file */
      //qsec=((struct BRAIN *)brainTerm->def)->qsec;
      x=itime/qsec;
      sprintf(newtime,"%11.11u",x*qsec);
      if(memcmp(newtime,header.time,11)>0){
        memcpy(header.time,newtime,11);
        memcpy(header.count,"000000",6);
        }
      else{
        update=0;
        strncpy(newtime,header.time,11);
        *(newtime+11)=0;
        }
      strncpy(newcount,header.count,6);
      *(newcount+6)=0;
      break;
    case NBQ_NEXT:      /* next queue file name */
      sprintf(newtime,"%11.11u",itime);
      strncpy(newcount,header.count,6);
      *(newcount+6)=0;
      if(memcmp(newtime,header.time,11)>0){
        memcpy(header.time,newtime,11);
        strcpy(newcount,"000000");
        }
      else{
        fcount=atoi(newcount)+1;
        sprintf(newcount,"%6.6u",fcount);
        }
      memcpy(header.count,newcount,6);
      break;
    case NBQ_UNIQUE:    /* unique queue file */
      sprintf(newtime,"%11.11u",itime);
      strncpy(newcount,header.count,6);
      *(newcount+6)=0;
      if(memcmp(newtime,header.time,11)>0){
        memcpy(header.time,newtime,11);
        strcpy(newcount,"000000");
        memcpy(header.count,newcount,6);
        fcount=1;
        }
      else fcount=atoi(newcount)+2;
      sprintf(newcount,"%6.6u",fcount);
      memcpy(header.count,newcount,6);
      sprintf(newcount,"%6.6u",fcount-1);
      break;
    default:
      outMsg(0,'L',"nbQueueGetFile() called with unrecognized option %u",option);
      nbQueueCloseFile(hFile);
      return(-1);
    }
  if(update){
    nbQueueSeekFile(hFile,0);
    if(nbQueueWriteFile(hFile,(char *)&header,sizeof(header))!=sizeof(header)){
      outMsg(0,'E',"Not able to write to queue header file");
      nbQueueCloseFile(hFile);
      return(-1);
      }
    }
  //outMsg(0,'T',"Closing control file");
  nbQueueCloseFile(hFile);
  if(type==' ') snprintf(filename,256,"%s.%s",newtime,newcount); //2012-01-16 dtl: used snprintf
  else snprintf(filename,256,"%s/%s/%s.%s.%c",dirname,identityName,newtime,newcount,type); //dtl used snprint
  return(0);
  }

/* 
*  This function is called by routines wanting to write to a queue
*     option=0 - provide a unique queue file name
*     option=1 - provide a time interval queue file based on brain definition
*     option<0 - provide a priority queue file name
*     type={q|c|t|p}
*/
int nbQueueGetNewFileName(char *qname,char *directory,int option,char type){
  char filename[256];
  int x,qi=120;  /* queue interval should come from brain definition */
  if(option<0) sprintf(filename,"00000000000.000000.%5.5d.",-option);
  else if(option>0){
    x=time(NULL)/qi; 
    sprintf(filename,"%11.11d.000000.00000.",x*qi);
    }
  else sprintf(filename,"%11.11u.%6.6d.%5.5d%c",(unsigned int)time(NULL),getpid(),rand()&0xffff,'%');
  sprintf(qname,"%s/%s%c",directory,filename,type);
  return(0);
  }

/*
*  Rename a queue file from a working name to a committed name.
*
*    00000000000.000000.00000%x  00000000000.000000.00000.x
*/
void nbQueueCommit(char *filename){
  char *cursor,newname[512];

  snprintf(newname,sizeof(newname),"%s",filename); //2012-01-16 dtl: replaced strcpy
  cursor=newname+strlen(newname)-2;
  if(cursor<newname || *cursor!='%'){
    outMsg(0,'L',"nbQueueCommit() unrecognized file name \"%s\"",newname);
    return;
    }
  *cursor='.';
  rename(filename,newname); 
  }

/* The close functions above should not free the qHandle */
void nbQueueCloseDir(struct NBQ_HANDLE *qHandle){
  struct NBQ_ENTRY *entry;
  if(qHandle->qfile!=NBQFILE_ERROR) nbQueueCloseFile(qHandle->qfile);
  while((entry=qHandle->entry)!=NULL){
    qHandle->entry=entry->next;
    nbFree(entry,sizeof(struct NBQ_ENTRY));
    }
  nbFree(qHandle,sizeof(struct NBQ_HANDLE));
  }
 
#if defined(WIN32)

/*
*  Get queue end-of-file
*     0 - eof set
*    -1 - error (file is closed and handle is freed)
*/
int nbqEof(qHandle) struct NBQ_HANDLE *qHandle;{
  unsigned int eof=qHandle->eof;

  qHandle->eof=SetFilePointer(qHandle->file,0,NULL,FILE_END);
  if(eof>0 && eof==qHandle->eof){
	qHandle->eof=0;
    qHandle->eof=SetFilePointer(qHandle->file,0,NULL,FILE_BEGIN);
    SetEndOfFile(qHandle->file);  
    }
  return(0);
  }

#else

int nbqEof(qHandle) struct NBQ_HANDLE *qHandle;{
  FILE *queue;
//  struct flock lock;
  long eof=qHandle->eof;

/*
  lock.l_whence=SEEK_SET;
  lock.l_start=2;
  lock.l_len=1;
  lock.l_type=F_WRLCK;
  if((rc=fcntl(qHandle->file,F_SETLKW,&lock))<0){ 
    outMsg(0,'E',"Unable to lock %s rc=%d errno=%d",qHandle->qname,rc,errno);
    nbQueueCloseFile(qHandle->file);
    return(-1);
    }
*/
  qHandle->eof=lseek(qHandle->file,0,SEEK_END); /* get end of file position */
  if(eof>0 && eof==qHandle->eof){  /* the file has not grown - let's blow it away */
    if((queue=fopen(qHandle->filename,"w"))==NULL){
      outMsg(0,'E',"Unable to open %s for write to empty.",qHandle->filename);
      nbQueueCloseFile(qHandle->file);
      return(-1);
      }
    fclose(queue);
    qHandle->eof=0;
    return(0);
    }
/*
  lock.l_type=F_UNLCK;
  if((rc=fcntl(qHandle->file,F_SETLK,&lock))<0){ 
    outMsg(0,'E',"Unable to unlock %s rc=%d errno=%d",qHandle->qname,rc,errno);
    nbQueueCloseFile(qHandle->file);
    return(-1);
    }
*/
  return(0);
  }

#endif
 
/*
*  Open a queue file for processing
*
*  Returns: -1 - error, 0 - busy, 1 file opened and locked
*/
#if defined(WIN32)

HANDLE *nbQueueOpenFileP(filename) char *filename;{
  HANDLE *file;
  OVERLAPPED olap;
  if((file=CreateFile(filename,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))==NULL){
    outMsg(0,'E',"Unable to open %s",filename);
    return(NULL);
    }
  olap.OffsetHigh=0;
  olap.hEvent=0;
  olap.Offset=1;
  if(!LockFileEx(file,LOCKFILE_FAIL_IMMEDIATELY|LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&olap)){
    outMsg(0,'T',"Queue file %s busy.",filename);
    CloseHandle(file);
    return(NULL);
    }
  return(file);
  }

#else

int nbQueueOpenFileP(filename) char *filename;{
  struct flock lock;
  int rc;
  int file;
#if defined(mpe) || defined(ANYBSD)
//2012-01-26 dtl: replaced open() with centralised routine openCreate()
  if((file=openCreate(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<=0){ //dtl replaced open
#else
  if((file=openCreate(filename,O_RDWR|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR))<=0){ //dtl replaced open
#endif
    outMsg(0,'E',"Unable to open %s",filename);
    return(-1);
    }
  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET;
  lock.l_start=0;
  lock.l_len=1;
  if((rc=fcntl(file,F_SETLK,&lock))<0){  /* get a lock only if someone else doesn't hold one */
    if(errno==EACCES || errno==EAGAIN){
      outMsg(0,'T',"Queue file %s busy.",filename);
      return(0);
      }
    outMsg(0,'E',"Unable to lock %s rc=%d errno=%d",filename,rc,errno);
    close(file);
    return(-1);
    }
  return(file);
  }

#endif

/*
*  Open Queue File
*
*  Returns:
*    -1 - error
*     0 - busy
*     1 - file opened
*/
int nbQueueOpenFile(struct NBQ_HANDLE *qHandle){
  struct NBQ_ENTRY *qEntry=qHandle->entry;
//#if !defined(WIN32)
//  struct flock lock;
//  int rc;
//#endif
  if(qEntry==NULL){
    outMsg(0,'L',"nbQueueOpenFile() called with empty handle");
    return(-1);
    }
  sprintf(qHandle->filename,"%s/%s/%s",qHandle->qname,qEntry->identity->name->value,qEntry->filename);
#if defined(WIN32) 
  if((qHandle->file=CreateFile(qHandle->filename,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))==NULL){
#else
#if !defined(mpe) && !defined(ANYBSD)
  if((qHandle->file=open(qHandle->filename,O_RDWR|O_SYNC))<0){
#else
  if((qHandle->file=open(qHandle->filename,O_RDWR))<0){
#endif
#endif
    outMsg(0,'E',"Unable to open %s",qHandle->filename);
    return(-1);
    }
  qHandle->markPos=0;
  qHandle->readPos=0;
  qHandle->eof=0;
  *(qHandle->buffer)=0;
  qHandle->cursor=qHandle->buffer;
  qHandle->bufend=qHandle->buffer;
  /*
  *  Get exclusive on queue for processing if nobody else is
  */
  if(nbQueueLock(qHandle->file,NBQ_TEST,NBQ_PRODUCER)<0){
    outMsg(0,'W',"Can not access %s at this time, file is busy.",qHandle->filename);
    nbQueueCloseFile(qHandle->file);
    return(0);
    } 
  /*
  *  Block writes and get EOF position using nbqEof()
  *    nbqEof() closes and destroys the handle on error conditions
  */
  if(nbqEof(qHandle)<0) return(-1);
  return(1);
  }

/*
*  Read a line from the queue
*/
char *nbQueueRead(struct NBQ_HANDLE *qHandle){
  char *cursor=qHandle->cursor;
  char *command=cursor;
  long lastReadPos=-1;
#if defined(WIN32)
  DWORD bytesRead,bytesWritten,bytesToRead;
#else
  long  bytesRead,bytesToRead;
#endif

  if(qHandle->bufend>qHandle->buffer){ /* if we've returned a command already - mark it */
#if defined(WIN32)
    SetFilePointer(qHandle->file,qHandle->markPos,NULL,FILE_BEGIN);
    if(!WriteFile(qHandle->file,"#",1,&bytesWritten,NULL)){
      outMsg(0,'E',"Unable to mark %s",qHandle->qname);
      nbQueueCloseFile(qHandle->file);
      return(NULL);
      }
#else
    if(lseek(qHandle->file,qHandle->markPos,SEEK_SET)!=qHandle->markPos){
      nbQueueCloseFile(qHandle->file);
      outMsg(0,'L',"lseek on %s failed",qHandle->qname);
      return(NULL);
      }
    if(write(qHandle->file,"#",1)!=1){
      nbQueueCloseFile(qHandle->file);
      outMsg(0,'L',"Command mark on %s failed",qHandle->qname);
      return(NULL);
      }
#endif
    }
  while(1){
    command=cursor;
    cursor=(char *)memchr(command,'\n',qHandle->bufend-command);
    if(cursor==NULL){
      if(*command!=0 && command==qHandle->buffer){
        /* debug loop problem */
        outMsg(0,'E',"Transaction length>%u in %s",NB_BUFSIZE,qHandle->qname);
        outMsg(0,'E',"NodeBrain terminating");
        exit(NB_EXITCODE_FAIL);
        }
      qHandle->readPos+=command-(qHandle->buffer);
      bytesToRead=(qHandle->eof)-(qHandle->readPos);
      if(bytesToRead<=0){
        if(nbqEof(qHandle)<0 || qHandle->eof==0) return(NULL);
        bytesToRead=(qHandle->eof)-(qHandle->readPos);
        }
      if(bytesToRead>NB_BUFSIZE) bytesToRead=NB_BUFSIZE;
      // 2009-05-25 eat - debugging loop problem
      if(qHandle->readPos<=lastReadPos){
        outMsg(0,'L',"nbQueueRead() none advancing read position");
        outMsg(0,'L',"NodeBrain terminating");
        exit(NB_EXITCODE_FAIL);
        }
#if defined(WIN32)
      SetFilePointer(qHandle->file,qHandle->readPos,NULL,FILE_BEGIN);
      if(!ReadFile(qHandle->file,qHandle->buffer,bytesToRead,&bytesRead,NULL)){
#else
      if(lseek(qHandle->file,qHandle->readPos,SEEK_SET)!=qHandle->readPos){
        nbQueueCloseFile(qHandle->file);
        outMsg(0,'L',"lseek on %s failed",qHandle->qname);
        return(NULL);
        }
      if((bytesRead=read(qHandle->file,qHandle->buffer,bytesToRead))<=0){
#endif
        outMsg(0,'E',"Unable to read %s",qHandle->qname);
        nbQueueCloseFile(qHandle->file);
        return(NULL);
        }
      qHandle->bufend=qHandle->buffer+bytesRead;
      qHandle->cursor=qHandle->buffer;
      cursor=qHandle->cursor;
      }
    else if(*command=='#') cursor++;
    else{
      *cursor=0;
      if(*(cursor-1)=='\r') *(cursor-1)=0;
      qHandle->cursor=cursor+1;
      qHandle->markPos=(qHandle->readPos)+command-(qHandle->buffer);
      return(command);
      }
    }

  }

/*
*  Add a file entry to a queue handle
*    We quietly ignore unrecognized file types
*/
static void nbqAddEntry(qHandle,identity,filename)
  struct NBQ_HANDLE *qHandle;
  struct IDENTITY   *identity;
  char   *filename; {

  char   *cursor;
  int    len;
  struct NBQ_ENTRY  *entry,**entryP;

  if((len=strlen(filename))<3 || *filename=='.') return;
  cursor=filename+len-2;
  if(*cursor!='.') return;
  if(*(cursor+1)=='Q') return;

  entry=(struct NBQ_ENTRY *)nbAlloc(sizeof(struct NBQ_ENTRY));
  entry->identity=identity;
  entry->context=NULL;              /* later */
  strcpy(entry->filename,filename);
  entry->type=*(cursor+1);

  /* insert in time order */
  for(entryP=&(qHandle->entry);*entryP!=NULL && strcmp((*entryP)->filename,filename)<0;entryP=&((*entryP)->next));
  entry->next=*entryP;
  *entryP=entry;
  }

/*
*  Read a queue directory into memory
*
*    siName is specified to limit the search to a specific identity
*    mode: 0 - share   1 - exclusive
*    
*/
struct NBQ_HANDLE *nbQueueOpenDir(char *dirname,char *siName,int mode){
  struct NBQ_HANDLE *qHandle;
  struct IDENTITY *identity;
//  char   iName[256];
  char   fence[256];
  NBQFILE qfile=NBQFILE_ERROR;
#if defined(WIN32)
  HANDLE lFile=NULL;
  HANDLE iDir;
  HANDLE fDir;
  WIN32_FIND_DATA ent;
  char   fSearchName[512];
#else
//  int    lFile=0;
  DIR    *iDir,*fDir;
  struct dirent *iEnt,*fEnt;
#endif
  char   iSearchName[512];
  if(trace) outMsg(0,'T',"nbQueueOpenDir() called");
  qHandle=nbAlloc(sizeof(struct NBQ_HANDLE));
  qHandle->pollSynapse=NULL;
  qHandle->yieldSynapse=NULL;
  strcpy(qHandle->qname,dirname);

  qHandle->entry=NULL;
  qHandle->markPos=0;
  qHandle->readPos=0;
  qHandle->eof=0;
  *(qHandle->buffer)=0;
  qHandle->cursor=NULL;
  qHandle->bufend=NULL;
  qHandle->qfile=qfile;

#if defined(WIN32)
  strcpy(iSearchName,qHandle->qname);
  strcat(iSearchName,"/*.*");
  iDir=FindFirstFile(iSearchName,&ent);
  if(iDir==INVALID_HANDLE_VALUE){
    if(GetLastError()==ERROR_NO_MORE_FILES) return(qHandle);
    outMsg(0,'E',"Unable to read queue %s - errno=%d",qHandle->qname,GetLastError());
    nbFree(qHandle,sizeof(struct NBQ_HANDLE));
    if(lFile) nbQueueCloseFile(lFile);
    return(NULL);
    }
  while(FindNextFile(iDir,&ent)){
    if(*ent.cFileName!='.'){ /* ignore names starting with '.' */
      strcpy(iName,ent.cFileName);
      if(identity=getIdentity(iName)){
        //if(nbQueueGetFile(fence,brainTerm,iName,NBQ_NEXT,' ')!=0){
        if(nbQueueGetFile(fence,dirname,iName,0,NBQ_NEXT,' ')!=0){
          outMsg(0,'E',"nbQueueOpenDir() not able to process header for %s/%s",qHandle->qname,iName);
          }
        else{
          strcpy(fSearchName,qHandle->qname);
          strcat(fSearchName,"/");
          strcat(fSearchName,iName);
          strcat(fSearchName,"/*.*");
          fDir=FindFirstFile(fSearchName,&ent);
          if(fDir!=INVALID_HANDLE_VALUE){
            if(strcmp(ent.cFileName,fence)<0) nbqAddEntry(qHandle,identity,ent.cFileName);
            while(FindNextFile(fDir,&ent))
              if(strcmp(ent.cFileName,fence)<0) nbqAddEntry(qHandle,identity,ent.cFileName);
            FindClose(fDir);
            }
          }
        }
      else{
        outMsg(0,'W',"Identity %s not recognized",iName);
        }
      }
    }
  if(GetLastError()==ERROR_NO_MORE_FILES){
    FindClose(iDir);
    return(qHandle);
    }
  FindClose(iDir);
  nbQueueCloseDir(qHandle);
  if(lFile) nbQueueCloseFile(lFile);
  return(NULL);
#else
  if((iDir=opendir(qHandle->qname))==NULL){
    outMsg(0,'E',"Unable to open %s",qHandle->qname);
    nbFree(qHandle,sizeof(struct NBQ_HANDLE));
    return(NULL);
    }
  while((iEnt=readdir(iDir))!=NULL){
    if(*(iEnt->d_name)!='.' && (siName==NULL || strcmp(siName,iEnt->d_name)==0) && (identity=getIdentity(iEnt->d_name))!=NULL){
      sprintf(iSearchName,"%s/%s",qHandle->qname,iEnt->d_name);
      //if(nbQueueGetFile(fence,brainTerm,iEnt->d_name,NBQ_NEXT,' ')!=0){
      if(nbQueueGetFile(fence,dirname,iEnt->d_name,0,NBQ_NEXT,' ')!=0){
        outMsg(0,'E',"nbQueueOpenDir() not able to process header for %s",iSearchName);
        }
      else if((fDir=opendir(iSearchName))!=NULL){
        while((fEnt=readdir(fDir))!=NULL){
          if(*(fEnt->d_name)!='.' && strcmp(fEnt->d_name,fence)<0) nbqAddEntry(qHandle,identity,fEnt->d_name);
          }
        closedir(fDir);
        }
      }
    }
  closedir(iDir);
  return(qHandle);
#endif
  }

/*
*  Process a "q" file
*/
static void nbqProcQ(struct NBQ_HANDLE *qHandle,nbCELL context){
  char *cmd;

  if(trace) outMsg(0,'T',"nbqProcQ() called for %s",qHandle->filename);
  if(qHandle->eof==0){
    if(trace) outMsg(0,'I',"File %s is empty",qHandle->filename);
    nbQueueCloseFile(qHandle->file);
    if(remove(qHandle->filename)<0) outMsg(0,'L',"Remove failed - %s",qHandle->filename);
    return;
    }
  outMsg(0,'I',"NBQ File %s",qHandle->filename);
  while((cmd=nbQueueRead(qHandle))!=NULL){
    nbCmdSid(context,cmd+1,1,qHandle->entry->identity);
    outFlush();
    }
  outMsg(0,'I',"NBQ File %s processed",qHandle->filename);
  nbQueueCloseFile(qHandle->file);
  if(remove(qHandle->filename)<0) outMsg(0,'L',"Remove failed - %s",qHandle->filename);
  }

/*
*  Process a "t" file - Stub
*/
void nbqProcT(struct NBQ_HANDLE *qHandle,nbCELL context){
  outMsg(0,'T',"nbqProcT() called for %s",qHandle->filename);
  nbQueueCloseFile(qHandle->file);
  }

/*
*  Process a "c" file - Stub
*/
void nbqProcC(struct NBQ_HANDLE *qHandle,nbCELL context){
  outMsg(0,'T',"nbqProcC() called for %s",qHandle->filename);
  nbQueueCloseFile(qHandle->file);
  }

//void nbqAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
//  struct NBQ_HANDLE *qHandle=(struct NBQ_HANDLE *)nodeHandle;

//static int nbqThread(struct NBQ_HANDLE *qHandle){
static int nbqThread(void *session){
  struct NBQ_HANDLE *qHandle=(struct NBQ_HANDLE *)session;
  nbCELL context=qHandle->context;

  struct NBQ_ENTRY  *qEntry;
  struct IDENTITY *saveClientIdentity=clientIdentity;

  qEntry=qHandle->entry;
  if(qEntry==NULL){
    if(qHandle->pollSynapse) nbCellEnable(qHandle->pollSynapse,NULL);
    nbQueueCloseDir(qHandle);
    return(1); // end thread
    }
  if(nbQueueOpenFile(qHandle)<=0)
    outMsg(0,'E',"Skipping %s",qHandle->filename);
  else{
    switch(qEntry->type){
      case 'q': nbqProcQ(qHandle,context); break;
      case 't': nbqProcT(qHandle,context); break;
      case 'c': nbqProcC(qHandle,context); break;
      }
    }
  qHandle->entry=qEntry->next;
  nbFree(qEntry,sizeof(struct NBQ_ENTRY));
  clientIdentity=saveClientIdentity;
  return(0);
  }

/*
*  Process a NodeBrain Queue
*/
//void nbQueueProcess(NB_Term *context,char *dirname,nbCELL synapse){
void nbQueueProcess(nbCELL context,char *dirname,nbCELL synapse){
  struct NBQ_HANDLE *qHandle;
  if(trace) outMsg(0,'T',"nbQueueProcess() called"); 
  if((qHandle=nbQueueOpenDir(dirname,NULL,0))==NULL){
    outMsg(0,'E',"Unable to process queue %s",dirname);
    return;
    }
  qHandle->context=context;
  qHandle->pollSynapse=synapse;
  if(synapse) nbCellDisable(synapse,NULL);
  //qHandle->yieldSynapse=nbSynapseOpen(context,NULL,qHandle,NULL,nbqAlarm);
  //nbSynapseSetTimer(context,qHandle->yieldSynapse,0);
  nbMedullaThreadCreate(nbqThread,qHandle);
  }
