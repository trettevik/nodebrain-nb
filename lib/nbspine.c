/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbspine.c 
*
* Title:    System API for NodeBrain Platform Independence
*
* Function:
*
*   This file provides functions that make up a platform independent API
*   to system services.  This file must not include or reference any
*   functions that require a NodeBrain context to make the API more easily
*   reused.
*
* Synopsis:
*
*   #include "nbspine.h"
*
*   nbCHILD nbChildOpen(int options,int uid,char *pgm,char *parms,cldin,cldout,clderr,char *msg,size_t msglen)
*
* Description
*
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/10/10 Ed Trettevik (split out from nbspawn.c in 0.6.3)
* 2005/12/12 eat 0.6.4  included options parameter to nbChild
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-10-16 eat 0.8.4  Changed order of setuid and setgid in nbChildOpen to do group first
* 2012-04-22 eat 0.8.8  Switched from nbcfg.h with standard config.h
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-10-13 eat 0.8.12 Switched to nb header
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nb.h>

// nbFileClose - maybe this should be a macro

void nbFileClose(nbFILE fildes){
#if defined(WIN32)
  CloseHandle(fildes);
#else
  close(fildes);
#endif
  }

// nbPipe

#if defined(WIN32)
//int nbPipeWin(HANDLE *write,HANDLE *read){
//  if(!CreatePipe(read,write,NULL,0))
//    return(1);  // create a pipe
//  return(0); 
//  }
int nbPipe(HANDLE *pipe1,HANDLE *pipe2){
  static int n=0;  
  char pipeName[32];
  n++;
  
  sprintf(pipeName,"\\\\.\\pipe\\nb%8.8d-%8.8d",getpid(),n); 
  //fprintf(stderr,"anonymous pipe is %s\n",pipeName);
  *pipe1=CreateNamedPipe(pipeName,
    PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_BYTE,
    1,              // two connections
    NB_BUFSIZE,     // input buffer size
    NB_BUFSIZE,     // output buffer size
    INFINITE,       // timeout
    NULL);          // security
  if(*pipe1==INVALID_HANDLE_VALUE){
    fprintf(stderr,"Unable to create named pipe\n");
    return(1);
    }
  *pipe2=CreateFile(pipeName,
    GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL);
  if(*pipe2==INVALID_HANDLE_VALUE){
    fprintf(stderr,"Unable to open named pipe\n");
    return(1);
    }
  return(0);
  }
#else
int nbPipe(nbFILE *writePipe,nbFILE *readPipe){
  int fdpair[2];
  if(pipe(fdpair)) return(-1);
  // 2012-12-27 eat 0.8.13 - CID 751528
  if(fcntl(fdpair[0],F_SETFD,FD_CLOEXEC)) // close on exec
    fprintf(stderr,"nbPipe: fcntl failed on readPipe\n");
  if(fcntl(fdpair[1],F_SETFD,FD_CLOEXEC))
    fprintf(stderr,"nbPipe: fcntl failed on writePipe\n");
  *readPipe=fdpair[0];
  *writePipe=fdpair[1];
  return(0);
  }
#endif

/*
*  Spawn a child process
*
*   int nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,cldin,cldout,clderr,char *msg,size_t msglen)
*
*   options - flag bits for controlling the type of child to create
*
*             NB_CHILD_SHELL     - pgm is a shell program
*
*             NB_CHILD_NOHUP     - signal(SIGHUP,SIG_IGN)  else SIG_DFT
*             NB_CHILD_NOCHLD    - signal(SIGCHLD,SIG_IGN) else SIG_DFT
*             NB_CHILD_NOTERM    - signal(SIGTERM,SIG_IGN) else SIG_DFT
*            
*             NB_CHILD_SESSION   - make leader of new process group
*             NB_CHILD_NOCLOSE   - Don't close files - I have it under control
*             NB_CHILD_SHOWCLOSE - Display a message when closing files
*
*   uid     - su user id
*
*   gid     - gu group id
*
*   pgm     - Program to execute
*
*             If NB_CHILD_SHELL is on
*               A null value for pgm will default to the local shell
*               command (e.g. /bin/sh or cmd.exe)
*             If NB_CHILD_SHELL is off
*               A null value for pgm will default to /usr/local/bin/nb
*             A null string "" is replaced by the name of the executing program
*
*   parms   - Parameter string
*
*             This string is parsed and converted into a null terminated
*             array of string pointers.
*
*   cldin   - Standard input file
*
*   cldout  - Standard output file
*
*   clderr  - Standard error file
*
*   msg     - message returned
*
*   msglen  - length of message return buffer
*
*
* Returns:
*   int pid
*/
#if defined(WIN32)
nbCHILD nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,nbFILE cldin,nbFILE cldout,nbFILE clderr,char *msg,size_t msglen){
  PROCESS_INFORMATION piProcInfo; 
  STARTUPINFO siStartInfo;
  char buffer[NB_BUFSIZE];
  char suffix[40];
  char pgmname[512];
  char shellparm[3];
  int  pgmnamesize;
  int  lasterror;
  nbCHILD child;
  BOOL inherit=FALSE;

// Set up members of the PROCESS_INFORMATION and STARTUPINFO structures. 
  ZeroMemory(&piProcInfo,sizeof(PROCESS_INFORMATION)); 
  ZeroMemory(&siStartInfo,sizeof(STARTUPINFO));
  siStartInfo.cb=sizeof(STARTUPINFO);
  siStartInfo.lpDesktop="";
  siStartInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
  siStartInfo.wShowWindow=SW_HIDE;
  if(cldin!=NULL){
    SetHandleInformation(cldin,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    siStartInfo.hStdInput=cldin;
    }
  if(cldout!=NULL){
    SetHandleInformation(cldout,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    siStartInfo.hStdOutput=cldout;
    }
  if(clderr!=NULL){
    SetHandleInformation(clderr,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    siStartInfo.hStdError=clderr;
    }
 
// Create the child process.
  if(pgm==NULL || *pgm==0){
    if(options&NB_CHILD_CLONE){
      pgmnamesize=GetModuleFileName(NULL,pgmname,sizeof(pgmname));
      if(pgmnamesize==sizeof(pgmname)){
        snprintf(msg,msglen,"Executing module name is too large for buffer.");
        return(NULL);
        }
      if(pgmnamesize==0){
        snprintf(msg,msglen,"GetModuleFileName errno=%d",GetLastError());
        return(NULL);
        }
      pgm=pgmname;
      }
    else if(options&NB_CHILD_SHELL) pgm="cmd";
    else pgm="nb";
    }
  if(options&NB_CHILD_SHELL) strcpy(shellparm,"/c");
  else *shellparm=0; 
  sprintf(buffer,"%s %s %s",pgm,shellparm,parms);

  if(options&NB_CHILD_NOCLOSE) inherit=TRUE;
  if(!CreateProcess(
      NULL, 
      buffer,        // command line 
      NULL,          // process security attributes 
      NULL,          // primary thread security attributes 
//      inherit,       // handles are inherited 
      TRUE,          // handles are inherited
      //CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP, // creation flags 
      CREATE_NEW_PROCESS_GROUP, // creation flags 
      NULL,          // use parent's environment 
      NULL,          // use parent's current directory 
      &siStartInfo,  // STARTUPINFO pointer 
      &piProcInfo)){  // receives PROCESS_INFORMATION 
    lasterror=GetLastError();
    switch(lasterror){
      case 2: snprintf(msg,msglen,"Executable file \"%s\" not found.",pgm); break;
      default: snprintf(msg,msglen,"CreateProcess() unable to create child process - system error code=%d",GetLastError());
      }
    return(NULL);
    }
  if(cldin!=NULL) CloseHandle(cldin);  
  if(cldout!=NULL) CloseHandle(cldout);
  if(clderr!=NULL) CloseHandle(clderr);
  child=nbAlloc(sizeof(NB_Child));
  child->handle=piProcInfo.hProcess;
  child->pid=piProcInfo.dwProcessId;
  //CloseHandle(piProcInfo.hProcess);
  CloseHandle(piProcInfo.hThread);
  snprintf(msg,msglen,"child pid[%d] out[%s]",piProcInfo.dwProcessId,suffix);
  //return(piProcInfo.dwProcessId);
  return(child);
  }
#else  
nbCHILD nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,nbFILE cldin,nbFILE cldout,nbFILE clderr,char *msg,size_t msglen){
  int pid,fd,closebit;
  char parmbuf[NB_BUFSIZE];
  int  argc=0,i;
  char *argv[20];
  char *cursor,*delim;
  nbCHILD child;

  if(!(options&NB_CHILD_SHELL) && strlen(parms)>=sizeof(parmbuf)){   // 2013-01-01 eat - VID 4613,5380,5423-0.8.13-1
    snprintf(msg,msglen,"Parm string is exceeds limit of %d - not spawning child",NB_BUFSIZE-1);
    return(NULL);
    }
  if((pid=fork())<0){
    snprintf(msg,msglen,"Unable to create child process - %s",strerror(errno));
    return(NULL);
    }
  if(pid>0){   // parent process
    snprintf(msg,msglen,"child pid %d",pid);
    close(cldin);  // close the files in the parent process
    close(cldout);
    close(clderr);
    child=nbAlloc(sizeof(NB_Child));
    child->pid=pid;
    return(child);
    }
  else{  // child process
    // 2012-12-27 eat 0.8.13 - CID 751527 - testing return from fcntl
    close(0);
    if(dup2(cldin,0)!=0) fprintf(stderr,"cldin dup to stdin failed\n"); 
    close(cldin);
    if(fcntl(0,F_SETFD,0)) fprintf(stderr,"fcntl failed on stdin\n");  // don't close on exec
    close(1);
    if(dup2(cldout,1)!=1) fprintf(stderr,"cldout dup to stdout failed\n");  
    close(cldout);
    if(fcntl(1,F_SETFD,0)) fprintf(stderr,"fcntl failed on stdout\n");
    close(2);
    if(dup2(clderr,2)!=2) fprintf(stdout,"clderr dup to stderr failed\n");
    close(clderr);
    if(fcntl(2,F_SETFD,0)) fprintf(stderr,"fcntl failed on stderr\n");

    // The calling program should make sure the close on exec flag is set
    // using fcntl(fd,F_SETFD) for files above 2. But we will clean them 
    // up here unless NB_CHILD_NOCLOSE is specified.
    if(!(options&NB_CHILD_NOCLOSE)){
      if(options&NB_CHILD_SHOWCLOSE){   // debugging option - write to child's stderr
        fprintf(stderr,"nbChildOpen(): showing open files\n");
        for(fd=getdtablesize();fd>2;fd--){
          closebit=fcntl(fd,F_GETFD);
          if(closebit<0){
            if(errno!=EBADF) perror("nbChildOpen() fcntl error");
            }
          else if(closebit==0){
            fprintf(stderr,"nbChildOpen(): closing open file %d\n",fd);
            close(fd);
            }
          else fprintf(stderr,"nbChildOpen(): open file %d has close bit set\n",fd);
          }
        }
      else for(fd=getdtablesize();fd>2;fd--) close(fd);
      }
    if(getuid()==0){  // if we are running as root, optionally set gid and uid
      if(gid!=0) setgid(gid); // set group id if requested
      if(uid!=0) setuid(uid); // set user id if requested
      }

    // switch major signals to SIG_IGN or SIG_DFT
    if(options&NB_CHILD_NOCHLD) signal(SIGCHLD,SIG_IGN);
    else signal(SIGCHLD,SIG_DFL);
    if(options&NB_CHILD_NOHUP) signal(SIGHUP,SIG_IGN);
    else signal(SIGHUP,SIG_DFL);
    if(options&NB_CHILD_NOTERM) signal(SIGTERM,SIG_IGN);
    else signal(SIGTERM,SIG_DFL);
    if(options&NB_CHILD_SESSION){
      //fprintf(stderr,"Making new process group leader\n");
      i=setsid();  // make it the leader of a new process group
      if(i<0) fprintf(stderr,"setsid() failed\n");
      //else fprintf(stderr,"setsid() was successful\n");
      }
    if(options&NB_CHILD_SHELL){
      if(pgm==NULL || *pgm==0) pgm="/bin/sh";
      execl(pgm,pgm,"-c",parms,NULL);  // 2013-01-01 eat - VID 784-0.8.13-1 Intentional user specified command
      }
    else{  // parse the program and parameters and build argv[]
      if(pgm==NULL || *pgm==0) pgm="/usr/local/bin/nb";
      // Here we have strange rules
      //   If a parameter starts with '"', it is delimited by an unescaped '"'
      //   otherwise it is delimited by a space and may contain quotes
      //   An unbalanced quote is delimited by end of string
      argv[argc]=pgm;
      argc++;
      strncpy(parmbuf,parms,sizeof(parmbuf)-1);  // Take a copy so we can chop it up with null terminators for each parm - length checked above
      *(parmbuf+sizeof(parmbuf)-1)=0;  // 2013-01-13 eat - checked above but helping the checker here
      cursor=parmbuf;          // 2013-01-01 eat - VID 4613,5380,5423-0.8.13-1 Changed this code to just work within parmbuf
      while(*cursor==' ') cursor++;
      while(*cursor){
        if(*cursor=='"'){
          cursor++;
          argv[argc]=cursor;
          delim=strchr(cursor,'"');
          if(!delim) delim=cursor+strlen(cursor); // this is actually a syntax error
          else while(delim>cursor && *(delim-1)=='\\'){ // handle escaped quotes
            strcpy(delim-1,delim);     // consume the escape - always have room
            delim=strchr(delim,'"');   // 2013-01-12 eat - looking beyond the previously found quote which moved left one byte
            if(!delim) delim=cursor+strlen(cursor);
            }
          }
        else{
          argv[argc]=cursor;
          delim=strchr(cursor,' ');
          if(!delim) delim=cursor+strlen(cursor);
          }
        cursor=delim;
        if(*cursor){
          *cursor=0;
          cursor++;
          while(*cursor==' ') cursor++;
          }
        argc++;
        }
      argv[argc]=NULL;
      execvp(pgm,argv);  // 2013-01-01 eat - VID 684-0.8.13-1 Intentional
      }
    _exit(0);
    }  
  }
#endif

nbCHILD nbChildClose(nbCHILD child){
#if defined(WIN32)
  CloseHandle(child->handle);
#endif
  nbFree(child,sizeof(NB_Child));
  return(NULL);
  }
