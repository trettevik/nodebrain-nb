/*
* Copyright (C) 1998-2009 The Boeing Company
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
* File:     nbspine.c 
*
* Title:    System API for NodeBrain Platform Independence
*
* Function:
*
*   This file provides functions that make up a platform independent API
*   to system services.  This file must have no dependence on a NodeBrain
*   environment.  It must compile without referencing any NodeBrain headers
*   except nbspine.h
*
* Synopsis:
*
*   #include "nbspine.h"
*
*   nbCHILD nbChildOpen(int options,int uid,char *pgm,char *parms,cldin,cldout,clderr,char *msg)
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
*=============================================================================
*/
//#include "nbstd.h"
#include <nbcfg.h>
#include <nbspine.h>

// 2005-12-12 eat 0.6.4 - not using sigaction
//#if !defined(WIN32)
//struct sigaction sigact;
//#endif

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
int nbPipe(int *writePipe,int *readPipe){
  int fdpair[2];
  if(pipe(fdpair)) return(-1);
  fcntl(fdpair[0],F_SETFD,FD_CLOEXEC); // close on exec
  fcntl(fdpair[1],F_SETFD,FD_CLOEXEC);
  *readPipe=fdpair[0];
  *writePipe=fdpair[1];
  return(0);
  }
#endif

/*
*  Spawn a child process
*
*   int nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,cldin,cldout,clderr,char *msg)
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
*
* Returns:
*   int pid
*/
#if defined(WIN32)
nbCHILD nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,nbFILE cldin,nbFILE cldout,nbFILE clderr,char *msg){
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
        sprintf(msg,"Executing module name is too large for buffer.");
        return(NULL);
        }
      if(pgmnamesize==0){
        sprintf(msg,"GetModuleFileName errno=%d",GetLastError());
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
      case 2: sprintf(msg,"Executable file \"%s\" not found.",pgm); break;
      default: sprintf(msg,"CreateProcess() unable to create child process - system error code=%d",GetLastError());
      }
    return(NULL);
    }
  if(cldin!=NULL) CloseHandle(cldin);  
  if(cldout!=NULL) CloseHandle(cldout);
  if(clderr!=NULL) CloseHandle(clderr);
  child=malloc(sizeof(NB_Child));
  child->handle=piProcInfo.hProcess;
  child->pid=piProcInfo.dwProcessId;
  //CloseHandle(piProcInfo.hProcess);
  CloseHandle(piProcInfo.hThread);
  sprintf(msg,"child pid[%d] out[%s]",piProcInfo.dwProcessId,suffix);
  //return(piProcInfo.dwProcessId);
  return(child);
  }
#else  
nbCHILD nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,nbFILE cldin,nbFILE cldout,nbFILE clderr,char *msg){
  int pid,fd,closebit;
  char parmbuf[NB_BUFSIZE];
  int  argc=0,i;
  char *argv[20];
  char *curStart,*curEnd,*curBuf;
  nbCHILD child;

  //outMsg(0,'T',"nbChildOpen() options=%x",options);
#if !defined(mpe) && !defined(ANYBSD)
  if((pid=vfork())<0){
#else
  if((pid=fork())<0){
#endif
    sprintf(msg,"Unable to create child process. errno=%d",errno);
    return(NULL);
    }
  if(pid>0){
    sprintf(msg,"child pid %d",pid);
    close(cldin);  // close the files in the parent process
    close(cldout);
    close(clderr);
    child=malloc(sizeof(NB_Child));
    child->pid=pid;
    return(child);
    }
  else{
    close(0);
    if(dup2(cldin,0)!=0) fprintf(stderr,"cldin dup to stdin failed\n"); 
    close(cldin);
    fcntl(0,F_SETFD,0);  // don't close on exec
    close(1);
    if(dup2(cldout,1)!=1) fprintf(stderr,"cldout dup to stdout failed\n");  
    close(cldout);
    fcntl(1,F_SETFD,0);
    close(2);
    dup2(clderr,2);
    close(clderr);
    fcntl(2,F_SETFD,0);

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

    if(uid!=0 && getuid()==0){   // set user id if we are root
      setuid(uid);
      setgid(gid);
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
      execl(pgm,pgm,"-c",parms,0);
      }
    else{  // parse the program and parameters and build argv[]
      if(pgm==NULL || *pgm==0) pgm="/usr/local/bin/nb";
      // Here we have strange rules
      //   If a parameter starts with '"', it is delimited by an unescaped '"'
      //   otherwise it is delimited by a space and may contain quotes
      //   An unbalanced quote is delimited by end of string
      curBuf=parmbuf;
      argv[argc]=pgm;
      argc++;
      curStart=parms; 
      while(*curStart==' ') curStart++;
      while(*curStart!=0){
        argv[argc]=curBuf;
        if(*curStart=='"'){
          curStart++;
          if((curEnd=strchr(curStart,'"'))==NULL)
            curEnd=strchr(curStart,0); // this is actually a syntax error
          while(curEnd!=curStart && *(curEnd-1)=='\\'){ // handle escaped quotes
            curEnd--;
            // We need to check for a stack buffer overflow here
            // if(curBuf+(curEnd-curStart)>(parmbuf+sizeof(parmbuf))) - error
            strncpy(curBuf,curStart,curEnd-curStart);
            curBuf+=curEnd-curStart;
            *curBuf='"';
            curBuf++;
            curStart=curEnd+2;
            curEnd=strchr(curStart,'"');
            }
          strncpy(curBuf,curStart,curEnd-curStart);
          }
        else{
          if((curEnd=strchr(curStart,' '))==NULL)
            curEnd=strchr(curStart,0);
          strncpy(curBuf,curStart,curEnd-curStart);
          }
        curBuf+=curEnd-curStart;
        *curBuf=0;
        curBuf++;
        if(*curEnd){
          curStart=curEnd+1;
          while(*curStart==' ') curStart++;
          }
        else curStart=curEnd;
        argc++;
        }
      argv[argc]=NULL;
      execvp(pgm,argv);
      }
    _exit(0);
    }  
  }
#endif

nbCHILD nbChildClose(nbCHILD child){
#if defined(WIN32)
  CloseHandle(child->handle);
#endif
  free(child);
  return(NULL);
  }
