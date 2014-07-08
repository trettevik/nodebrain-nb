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
* Program:  nodebrain
*
* File:     nbmedulla.c 
* 
* Title:    NodeBrain Medulla API
*
* Function:
*
*   This file provides Medulla API routines used by main programs to manage
*   the timing of I/O and scheduled events to avoid blocking on I/O.  This
*   interface is based on the select() function on Linux and UNIX systems.  It
*   is based on WaitForMultipleObjects() on Windows.
*   
* Synopsis:
*
*   #include <nbmedulla.h>
*
*   nbMEDULLA nbMedullaProcessClose(nbMEDULLA medulla);
*   int nbMedullaWaitEnable(int type,int fildes,void *session,(*handler)(int fildes,void *session));
*   int nbMedullaWaitDisable(int type,int fildes);
*   int nbMedullaPulse();
*   int nbMedullaStop();
*
* Description
*
*   This interface simplifies the creation of an application that exchanges
*   information with child processes and socket connections.
*
*   The name "medulla" comes from brain structure.  It is part of the brain
*   stem between the spinal cord and the process.  It is resprocessible for breathing
*   digestion and heart rate.
*
*   nbMedullaProcessOpen()
*
*   The call to open specifies a routine to handle scheduled events.  It is
*   called to get the duration until the next scheduled event before waiting
*   on I/O.  If this time passes before I/O is ready, the scheduler is
*   called again.  The scheduler is resprocessible for performing scheduled
*   events and then returning with the duration until the next schedule event.
*   
*   A handle (nbMEDULLA) is returned for use in all other medulla routines.
*
*   nbMedullaProcessClose()
*
*   The routine is used to free up the space used for control structures.  A
*   NULL value is returned.
*
*   nbMedullaWaitEnable()
*
*   The enable function is called to identify a handler routine for a specific
*   event (0 - read, 1 - write, 2 - exception) on a specific file descriptor. 
*
*   nbMedullaWaitDisable()
*
*   The disable function causes the medulla to stop watching for events of a
*   specific type on a specific file.
*   
*   nbMedullaPulse()
*
*   The start routine goes into a select() loop calling the scheduler and I/O
*   exit routines as appropriate each time through the loop.  This routine
*   will not return until one of the handlers calls the stop routine.
*
*   nbMedullaStop()
*
*   This function is called by one of the handler routines to force the
*   call to the start routine to return.
*   
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/12/08 Ed Trettevik (introduced in 0.6.4)
* 2007/12/26 eat 0.6.8  included process closer exit for session cleanup
* 2008/09/30 eat 0.7.2  included medulla thread support
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010/02/28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2010-10-16 eat 0.8.4  Included group option in shell commands
*                       =[<user>:<group>]<command>
* 2012-01-09 dtl 0.8.6  Checker updates
* 2012-04-22 eat 0.8.8  Switched from nbcfg.h to standard config.h
* 2012-05-29 eat 0.8.10 Fixed to recognize buffer size limitation problem in nbMedullaQueueGet
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-19 eat 0.8.13 Resolved a problem with blocking servants totally blocking
*            The nbMedullaProcessReadBlocking function was for some crazy reason
*            doing a blocking read on stderr and then stdout.  This required the
*            process to end before stdout was read.  If the process wrote enough
*            to stdout to fill buffers and block waiting for the medulla to read
*            it we had a deadly embrace.  Now stdout and stderr are read concurrently
*            and the problem is resolved.
*=============================================================================
*/
#define NB_INTERNAL
#if defined(WIN32)
#include <nbcfgw.h>
#else
#include <config.h>
#endif
#include <nb/nbi.h>
#include <nb/nbmedulla.h>

nbMEDULLA nb_medulla=NULL;
nbPROCESS nb_process=NULL;      // list of child processes
int nb_medulla_child_count=0;   // number of children
int nb_medulla_child_max=50;    // maximum number of children
int nb_medulla_sigchld=0;       // flag indicating a child has ended

struct NB_MEDULLA_BUFFER *nb_free_buffer;
int nb_free_buffer_count=0;
int nb_free_buffer_max=16;

int nbMedullaProcessWriter(void *session);
int nbMedullaProcessReader(void *session);
int nbMedullaProcessLogger(void *session);
int nbMedullaProcessFileConsumer(void *session,char *msg);
int nbMedullaProcessFileLogger(void *session,char *msg);

// Medulla events are a workaround on Windows
// Unfortunately I haven't stumbled onto a clean way of implementing
// the Medulla on Windows because the mechanisms are too restricted.
// by file type.  We are using threads to implement asynchronous I/O
// for stdin and stdout.  The Medulla event scheme provides thread
// synchronization, forcing all processing except waiting around for
// I/O into the primary thread.
//
// This is just hacked out for the stdout message consumer, so event
// handlers must conform to the consumer's arguments.
//
//     int (*handler)(void *session,char *msg)
//
#if defined(WIN32)
CRITICAL_SECTION nb_medulla_event_section;
struct NB_MEDULLA_EVENT{
  struct NB_MEDULLA_EVENT *next;
  void *session;
  char msg[1024];
  int (*handler)(void *session,char *msg);
  };
struct NB_MEDULLA_EVENT *nb_medulla_event_used=NULL;
struct NB_MEDULLA_EVENT *nb_medulla_event_free=NULL;
HANDLE nb_medulla_event;
void nbMedullaEventInit(){
  //InitializeCriticalSection(&nb_medulla_event_section);
  if(!InitializeCriticalSectionAndSpinCount(&nb_medulla_event_section,0x80000400)){ 
    fprintf(stderr,"nbMedullaEventInit() - unable to intialize critical section - aborting\n");
    exit(NB_EXITCODE_FAIL);    
    }
  nb_medulla_event=CreateEvent( 
    NULL,    // default security attribute 
    TRUE,    // manual-reset event 
    FALSE,   // initial state = unsignaled 
    NULL);   // unnamed event object 
  }  
void nbMedullaEventSchedule(void *session,char *msg,int (*handler)(void *session,char *msg)){
  struct NB_MEDULLA_EVENT *event;
  __try{
    EnterCriticalSection(&nb_medulla_event_section);
    if((event=nb_medulla_event_free)==NULL) event=nbAlloc(sizeof(struct NB_MEDULLA_EVENT));
    else nb_medulla_event_free=event->next;
    event->next=nb_medulla_event_used;
    event->session=session;
    *event->msg=0;
    event->handler=handler;
    if(msg!=NULL){
      if(strlen(msg)<sizeof(event->msg)) strcpy(event->msg,msg);
      else fprintf(stderr,"nbMedullaEventSchedule() - msg too long: %s\n",msg);
      }
    nb_medulla_event_used=event;
    SetEvent(nb_medulla_event); // Notify wait loop in nbMedullaPulse()
    }
  __finally{
    LeaveCriticalSection(&nb_medulla_event_section);    
    }
  }
int nbMedullaEventProcess(void *session){
  struct NB_MEDULLA_EVENT *event;
  __try{
    EnterCriticalSection(&nb_medulla_event_section);
    while((event=nb_medulla_event_used)!=NULL){
      nb_medulla_event_used=event->next;
      (event->handler)(event->session,event->msg);
      event->next=nb_medulla_event_free;
      nb_medulla_event_free=event;
      }
    ResetEvent(nb_medulla_event); // Reset event 
    }
  __finally{
    LeaveCriticalSection(&nb_medulla_event_section);    
    }
  return(0);
  }
#endif

void nbMedullaProcessLimit(int limit){
  nb_medulla_child_max=limit;
  }

struct NB_MEDULLA_BUFFER *nbMedullaBufferAlloc(){
  struct NB_MEDULLA_BUFFER *buf;
  //fprintf(stderr,"nbMedullaBufferAlloc\n");
  if((buf=nb_free_buffer)==NULL){
    buf=nbAlloc(sizeof(struct NB_MEDULLA_BUFFER));
    buf->page=nbAlloc(NB_BUFSIZE);
    buf->end=buf->page+NB_BUFSIZE;
    }
  else{
    nb_free_buffer=buf->next;
    nb_free_buffer_count--;
    }
  buf->next=NULL;
  buf->data=buf->page;
  buf->free=buf->page;
  return(buf);
  }

void nbMedullaBufferFree(struct NB_MEDULLA_BUFFER *buf){
  //fprintf(stderr,"nbMedullaBufferFree\n");
  if(nb_free_buffer_count<nb_free_buffer_max){
    buf->next=nb_free_buffer;
    nb_free_buffer=buf;
    nb_free_buffer_count++;
    return;
    }
  nbFree(buf->page,NB_BUFSIZE);
  nbFree(buf,sizeof(struct NB_MEDULLA_BUFFER));
  }

void nbMedullaExit(void){
  static int finished=0;
  nbPROCESS process=nb_process;
  if(finished) return;  // allow multiple calls but only do it once
  else finished=1;
  if(process==NULL) return;
  for(process=process->next;process!=nb_process;process=process->next){
    if(process->options&NB_CHILD_TERM) nbMedullaProcessTerm(process);
    }
  }

/*
void nbMedullaTerminate(void){
  nbPROCESS process=nb_process;
  fprintf(stderr,"nbMedullaTerminate called\n");
  fflush(stderr);
  if(process==NULL) return;
  for(process=process->next;process!=nb_process;process=process->next){
    if(process->options&NB_CHILD_TERM){
      if(TerminateProcess(process->child->handle,1)==0)
        fprintf(stderr,"TerminateProcess() failed for %d error=%d\n",process->pid,GetLastError());
      else fprintf(stderr,"TerminateProcess() successful for %d\n",process->pid);
      }
    }
  fprintf(stderr,"nbMedullaTerminate finished\n");
  fflush(stderr);
  }
*/

void nbMedullaSigChildHandler(int sig){
  nb_medulla_sigchld=1;
  }



// Medulla process handler 
//
//   This function gets the completion code for processes that end
//   

#if defined(WIN32)

int nbMedullaProcessHandler(void *session){
  nbPROCESS process=(nbPROCESS)session;
  HANDLE hProcess=process->child->handle;

  if(GetExitCodeProcess(hProcess,&process->exitcode)==STILL_ACTIVE) return(1);
  strcpy(process->exittype,"Exit");
  nbMedullaWaitDisable(process->child->handle);
  process->status|=NB_MEDULLA_PROCESS_STATUS_ENDED;
  if(process->getpipe==NULL && process->logpipe==NULL) nbMedullaProcessClose(process);  
  return(0);
  }

#else

int nbMedullaProcessHandler(int final){
  int pid=1,status,count=0,waitopt=WNOHANG;
  nbPROCESS process;

  if(final) waitopt=0;
  else if(!nb_medulla_sigchld) return(0);
    
  pid=waitpid(-1,&status,waitopt);
  while(pid==-1 && errno==EINTR) pid=waitpid(-1,&status,waitopt);
  while(pid>0){
    count++;
    kill(-pid,SIGHUP);                    // SIGHUP the process group
    process=nbMedullaProcessFind(pid);
    if(process!=NULL){                    // if we have a process for this pid
      if(WIFEXITED(status)){
        strcpy(process->exittype,"Exit");
        process->exitcode=WEXITSTATUS(status);
        if(final) nb_medulla->processHandler(process,pid,"Ended",WEXITSTATUS(status));
        }
      else if(WIFSIGNALED(status)){
        strcpy(process->exittype,"Kill");
        process->exitcode=WTERMSIG(status);
        if(final) nb_medulla->processHandler(process,pid,"Killed",WTERMSIG(status));
        }
/* We haven't requested stop or continue status
      else if(WIFSTOPPED(status)){
        strcpy(process->exittype,"Stop");
        process->exitcode=WSTOPSIG(status);
        if(final) nb_medulla->processHandler(process,pid,"Stopped",WSTOPSIG(status));
        }
*/
      //fprintf(stderr,"nbMedullaProcessHandler - checking putfile\n");
      if(process->putfile>=0){      // clean up output queue right now
        //fprintf(stderr,"nbMedullaProcessHandler - closing putfile\n");
        // 2007-12-26 - eat - moved next line here from bottom of this section
        nbMedullaWaitDisable(1,process->putfile);
        close(process->putfile);
        process->putfile=-1;
        if(process->putQueue!=NULL) process->putQueue=nbMedullaQueueClose(process->putQueue);
        }
      process->status|=NB_MEDULLA_PROCESS_STATUS_ENDED; // flag ended status
      // wait to clean up input queues because we may have another read to do
      if(!final && process->getfile<0 && process->logfile<0) nbMedullaProcessClose(process); 
      }
    pid=waitpid(-1,&status,waitopt);
    while(pid==-1 && errno==EINTR) pid=waitpid(-1,&status,waitopt);
    }
  if(pid<0 && errno!=ECHILD){
    printf("waitpid failed errno=%d",errno);
    perror("Explain");
    }
  nb_medulla_sigchld=0;
#if defined(HPUX) || defined(SOLARIS)    // need to reestablish the handler on some systems
  signal(SIGCHLD,nbMedullaSigChildHandler);
#endif
  return(count);
  }

#endif

// Wait for a process to end and return the exit code.
#if defined(WIN32)

int nbMedullaProcessWait(nbPROCESS process){
  nbMedullaProcessReadBlocking(process);  // read stdin and stdout using blocking IO
  if(WaitForSingleObject(process->child->handle,1000*10)==WAIT_TIMEOUT){
    fprintf(stderr,"Timeout waiting for child to end\n");
    nbMedullaWaitEnable(process->child->handle,process,nbMedullaProcessHandler);
    return(1);
    }
  if(GetExitCodeProcess(process->child->handle,&process->exitcode)==STILL_ACTIVE){
    fprintf(stderr,"Child still active\n");
    nbMedullaWaitEnable(process->child->handle,process,nbMedullaProcessHandler);
    return(1);
    }
  strcpy(process->exittype,"Exit");
  process->status|=NB_MEDULLA_PROCESS_STATUS_ENDED;
  nbMedullaProcessClose(process); 
  return(0);
  }

#else

int nbMedullaProcessWait(nbPROCESS process){
  int pid,status;

  nbMedullaProcessReadBlocking(process);  // read stdin and stdout using blocking IO
  pid=waitpid(process->child->pid,&status,0);
  while(pid==-1 && errno==EINTR) pid=waitpid(process->child->pid,&status,0);
  if(pid<0){
    if(errno!=ECHILD){
      fprintf(stderr,"waitpid failed with errno=%d\n",errno);
      perror("Explain");
      return(0); // let it signal nbMedullaProcessHandler
      }
    else fprintf(stderr,"waitpid process not a child\n");
    }
  else{
    kill(-pid,SIGHUP);                    // SIGHUP the process group
    if(WIFEXITED(status)){
      strcpy(process->exittype,"Exit");
      process->exitcode=WEXITSTATUS(status);
      //nb_medulla->processHandler(process,pid,"Ended",WEXITSTATUS(status));
      }
    else if(WIFSIGNALED(status)){
      strcpy(process->exittype,"Kill");
      process->exitcode=WTERMSIG(status);
      //nb_medulla->processHandler(process,pid,"Killed",WTERMSIG(status));
      }
    else if(WIFSTOPPED(status)){
      strcpy(process->exittype,"Stop");
      process->exitcode=WSTOPSIG(status);
      //nb_medulla->processHandler(process,pid,"Stopped",WSTOPSIG(status));
      }
    }
  //fprintf(stderr,"nbMedullaProcessWait - checking putfile\n");
  if(process->putfile>=0){      // clean up output queue right now
    //fprintf(stderr,"nbMedullaProcessWait - closing putfile\n");
    nbMedullaWaitDisable(1,process->putfile);
    close(process->putfile);
    process->putfile=-1;
    if(process->putQueue!=NULL) process->putQueue=nbMedullaQueueClose(process->putQueue);
    }
  process->status|=NB_MEDULLA_PROCESS_STATUS_ENDED; // flag ended status
  nbMedullaProcessClose(process);
  return(0);
  }

#endif

// Handle Ctrl-C, Ctrl-Close, Ctrl-Break, Ctrl-Logoff, Ctrl-Shutdown on Windows.

#if defined(WIN32)
BOOL nbCtrlHandler(DWORD fdwCtrlType){
  char *event;
  switch(fdwCtrlType){
    case CTRL_C_EVENT:      event="CTRL_C_EVENT"; break;
    case CTRL_BREAK_EVENT:  event="CTRL_BREAK_EVENT"; break;
    case CTRL_CLOSE_EVENT:  event="CTRL_CLOSE_EVENT"; break;
    case CTRL_LOGOFF_EVENT: event="CTRL_LOGOFF_EVENT"; break;
    case CTRL_SHUTDOWN_EVENT:
      event="CTRL_SHUTDOWN_EVENT";
      return(FALSE);  // don't worry the system is going down anyway
    default: event="Unknown CTRL Event";
    }
  fprintf(stderr,"Received %s - terminating\n",event);
  // The next line is unnecessary for CTRL_CLOSE_EVENT and nbMedullaProcessTerm() calls will fail anyway
  nbMedullaExit();  // clean up processes if not a service
  return(FALSE);  // Continue on with normal response - exit  
  }
#endif
 
// Open a medulla

int nbMedullaOpen(void *session,int (*scheduler)(void *session),int (*processHandler)(nbPROCESS process,int pid,char *exittype,int exitcode)){
  //struct sigaction sigact;
  NB_Thread *thread;

#if defined(WIN32)
  nbMedullaEventInit();
#endif
  nb_medulla=nbAlloc(sizeof(struct NB_MEDULLA));
#if defined(WIN32)
  nb_medulla->waitCount=0;
#else
  FD_ZERO(&nb_medulla->readfds);
  FD_ZERO(&nb_medulla->writefds);
  FD_ZERO(&nb_medulla->exceptfds); 
  nb_medulla->handler=NULL;
  nb_medulla->handled=NULL;
#endif
  nb_medulla->session=session;
  nb_medulla->scheduler=scheduler;
  nb_medulla->processHandler=processHandler;
  nb_medulla->serving=0;

  // Initialize a header entry for the thread list
  thread=nbAlloc(sizeof(NB_Thread));
  thread->handler=NULL;
  thread->session=NULL;
  thread->next=thread;
  thread->prior=thread;
  nb_medulla->thread=thread;
  nb_medulla->thread_count=0;

  nb_process=nbAlloc(sizeof(struct NB_MEDULLA_PROCESS));
  nb_process->pid=getpid();
#if defined(WIN32)
  nb_process->getpipe=nbMedullaFileOpen(1,GetStdHandle(STD_INPUT_HANDLE),nb_process,nbMedullaProcessWriter);
  nb_process->putpipe=nbMedullaFileOpen(1,GetStdHandle(STD_OUTPUT_HANDLE),nb_process,nbMedullaProcessReader);
  nb_process->logpipe=NULL;
#else
  nb_process->getfile=0;
  nb_process->putfile=1;
  nb_process->logfile=-1;
  nb_process->getQueue=nbMedullaQueueOpen();
  nb_process->putQueue=nbMedullaQueueOpen();
  nb_process->logQueue=NULL;
#endif
  strncpy(nb_process->cmd,"root",sizeof(nb_process->cmd));
  nb_process->next=nb_process;
  nb_process->prior=nb_process;

// on windows use WaitForSingleObject
#if !defined(WIN32)
  signal(SIGCHLD,nbMedullaSigChildHandler);
#endif
  // Here's how we might do this with sigaction() if we decide it is better than signal() for our needs
  //sigaction(SIGCLD,NULL,&sigact);
  //sigact.sa_handler=nbMedullaSigChildHandler;
  //sigaction(SIGCLD,&sigact,NULL);

// don't do this while debugging a windows service problem
#if !defined(WIN32)
  atexit(nbMedullaExit);
#endif
#if defined(WIN32)
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)nbCtrlHandler,TRUE);
#endif
  return(0);
  }

// Close a medulla

int nbMedullaClose(void){
  return(0);
  }

// Enable/Disable wait/file handlers

#if defined(WIN32)

// Enable wait handler

int nbMedullaWaitEnable(HANDLE handle,void *session,NB_MEDULLA_WAIT_HANDLER handler){
  if(nb_medulla->waitCount>=NB_MEDULLA_WAIT_OBJECTS){
    fprintf(stderr,"nbMedullaWaitEnable() - too many wait objects - max=%d",NB_MEDULLA_WAIT_OBJECTS);
    return(1);
    }
  nb_medulla->waitObject[nb_medulla->waitCount]=handle;
  nb_medulla->waitSession[nb_medulla->waitCount]=session;
  nb_medulla->waitHandler[nb_medulla->waitCount]=handler;
  nb_medulla->waitCount++;
  return(0);
  }

void nbMedullaWaitRemove(int waitIndex){
  nb_medulla->waitCount--;
  while(waitIndex<nb_medulla->waitCount){
    nb_medulla->waitObject[waitIndex]=nb_medulla->waitObject[waitIndex+1];
    nb_medulla->waitSession[waitIndex]=nb_medulla->waitSession[waitIndex+1];
    nb_medulla->waitHandler[waitIndex]=nb_medulla->waitHandler[waitIndex+1];
    waitIndex++;        
    }
  }

// Disable wait handler

int nbMedullaWaitDisable(HANDLE handle){
  int waitIndex;
  for(waitIndex=0;waitIndex<nb_medulla->waitCount;waitIndex++){
    if(nb_medulla->waitObject[waitIndex]==handle){
      nbMedullaWaitRemove(waitIndex);    
      return(0);
      }
    }
  return(1);  // didn't find handle
  }
#else
// Enable a file handler

// 2007-12-27 eat - Modified to support update to existing entry
//                  A disabled entry may still be here and get replaced as well.

//int nbMedullaWaitEnable(int type,nbFILE fildes,void *session,int (*newHandler)(nbFILE fildes,void *session)){
int nbMedullaWaitEnable(int type,nbFILE fildes,void *session,NB_MEDULLA_WAIT_HANDLER handler){
  struct NB_MEDULLA_WAIT *medfile;
  fd_set *setP;

  for(medfile=nb_medulla->handler;medfile!=NULL && (type!=medfile->type || fildes!=medfile->fildes);medfile=medfile->next);
  if(medfile==NULL){
    if((medfile=nb_medulla->handled)==NULL) medfile=nbAlloc(sizeof(struct NB_MEDULLA_WAIT));
    else nb_medulla->handled=nb_medulla->handled->next;
    medfile->type=type;
    medfile->fildes=fildes;
    medfile->next=nb_medulla->handler;
    nb_medulla->handler=medfile;
    }
  medfile->close=0;
  medfile->session=session;
  medfile->handler=handler;
  switch(medfile->type){
    case 0: setP=&nb_medulla->readfds;   break;
    case 1: setP=&nb_medulla->writefds;  break;
    case 2: setP=&nb_medulla->exceptfds; break;
    default: 
      printf("nbMedullaWaitEnable: Logic error - invalid medulla file handler type=%d\n",medfile->type);
      exit(NB_EXITCODE_FAIL);
    }
  FD_SET(medfile->fildes,setP);
  // 2010-01-02 eat - see if we can make write waits return when the peer closes the connection
  //if(medfile->type==1) FD_SET(medfile->fildes,&nb_medulla->readfds);
  if(medfile->fildes>=nb_medulla->highfd) nb_medulla->highfd=medfile->fildes+1;
  return(0);
  }

// Disable a file handler

int nbMedullaWaitDisable(int type,nbFILE fildes){
  struct NB_MEDULLA_WAIT *handler;
  fd_set *setP;

  //fprintf(stderr,"nbMedullaWaitDisable(): type=%d fildes=%d\n",type,fildes);
  for(handler=nb_medulla->handler;handler!=NULL && (type!=handler->type || fildes!=handler->fildes);handler=handler->next);
  if(handler!=NULL){
    switch(handler->type){
      case 0: setP=&nb_medulla->readfds;   break;
      case 1: setP=&nb_medulla->writefds;  break;
      case 2: setP=&nb_medulla->exceptfds; break;
      default:
        printf("nbMedullaWaitDisable: Logic error - invalid medulla file handler type=%d\n",handler->type);
        exit(NB_EXITCODE_FAIL);
      }
    FD_CLR((unsigned int)handler->fildes,setP);
    // 2010-01-02 eat - see if we can make write waits return when the peer closes the connection
    //if(handler->type==1) FD_CLR(handler->fildes,&nb_medulla->readfds);
    handler->close=1;   // flag for removal 
    //fprintf(stderr,"nbMedullaWaitDisable(%d,%d): found and flagged\n",type,fildes);
    }
  return(0);
  }
#endif

void nbMedullaThreadServe(void);

// Start a medulla pulse
//
// nbMedullaPulse(serve);
//
//   serve - 0 - no, just do what is ready now and return
//           1 - yes, run in server mode
//
// There is a fundamental difference between the Windows and
// UNIX implementations.  These implementations have different
// parameters for nbMedullaWaitEnable().
//
// Under UNIX/Linux we depend on signals to interrupt the select()
// function to know when child processes end and we only directly
// watch for files ready for I/O with the select() function.
//
// Under Windows we watch for file I/O completion and child process
// completion using WaitForMultpleObjects().
//
#if defined(WIN32)
int nbMedullaPulse(int serve){
  int waitIndex,waitSeconds;

  // Enable wait on Medulla event queue used by asynchronous I/O threads
  // Disable this wait before returning
  nbMedullaWaitEnable(nb_medulla_event,NULL,nbMedullaEventProcess);
  nb_medulla->serving=1;
  while(nb_medulla->serving){
    if(nb_medulla->threadcount) nbMedullaThreadServe();
    if(serve){
      waitSeconds=(nb_medulla->scheduler)(nb_medulla->session);
      // 2007-07-22 eat 0.6.8 - included test of serving switch to respond to stop command
      if(waitSeconds<0 || nb_medulla->serving==0){ // heatbeat routine requesting stop or nbMedullaStop() called
        nb_medulla->serving=0;
        nbMedullaWaitDisable(nb_medulla_event);
        return(0);
        }
      if(nb_medulla->thread_count) waitSeconds=0;
      // do we need to check for minimum waitSeconds?
      }
    else waitSeconds=0; 
    // The waitCount should always be greater than 0 if we are using
    // a wait on the Medulla event queue.  However, we include a test
    // to avoid an error when the waitCount is 0.
    if(nb_medulla->waitCount>0){
      waitIndex=WaitForMultipleObjects( 
        nb_medulla->waitCount,   // number of event objects 
        nb_medulla->waitObject,  // array of event objects 
        FALSE,                   // does not wait for all 
        waitSeconds*1000);       // wait until scheduled event
      }
    else{
      Sleep(waitSeconds*1000);
      waitIndex=WAIT_TIMEOUT;
      }
    if(waitIndex==WAIT_FAILED){
      fprintf(stderr,"nbMedullaPulse() wait failed errno=%d\n",GetLastError());
      nb_medulla->serving=0;
      }
    else if(waitIndex==WAIT_TIMEOUT){
      if(!serve) nb_medulla->serving=0;
      }
    else{
      waitIndex-=WAIT_OBJECT_0;
      if(waitIndex<0 || waitIndex>=nb_medulla->waitCount){
        fprintf(stderr,"nbMedullaPulse() waitIndex %d out of bounds - terminating",waitIndex);
        exit(NB_EXITCODE_FAIL);
        }
      if((nb_medulla->waitHandler[waitIndex])(nb_medulla->waitSession[waitIndex])){
        // remove wait when handler requests removal with non-zero return
        nbMedullaWaitRemove(waitIndex);
        }
      }
    }
  nbMedullaWaitDisable(nb_medulla_event);
  return(0);
  }
#else
int nbMedullaPulse(int serve){
  struct NB_MEDULLA_WAIT *handler,**handlerP;
  struct timeval tv,tvNow;
  int readyfd;
  fd_set *setP;

  //fprintf(stderr,"nbMedullaPulse(%d) called\n",schedule);
  nb_medulla->serving=1;
  while(nb_medulla->serving){
    if(nb_medulla->thread_count) nbMedullaThreadServe();
    if(serve){
      tv.tv_sec=(nb_medulla->scheduler)(nb_medulla->session);
      // 2007-07-22 eat 0.6.8 - included test of serving switch to respond to stop command
      if(tv.tv_sec<0 || nb_medulla->serving==0){ // heatbeat routine requesting stop or nbMedullaStop() called
        nb_medulla->serving=0;
        return(0);
        }
      if(nb_medulla->thread_count) tv.tv_sec=0,tv.tv_usec=0;
      else{
        gettimeofday(&tvNow,NULL);  // align to start of second
        tv.tv_sec--;
        tv.tv_usec=1000000-tvNow.tv_usec;
        }
      }
    else tv.tv_sec=0,tv.tv_usec=0;
    //readyfd=select(nb_medulla->highfd,&nb_medulla->readfds,&nb_medulla->writefds,&nb_medulla->exceptfds,&tv);
    //fprintf(stderr,"select highfd=%d sec=%d\n",nb_medulla->highfd,tv.tv_sec);
    readyfd=select(nb_medulla->highfd,&nb_medulla->readfds,&nb_medulla->writefds,NULL,&tv);
    //fprintf(stderr,"select returned readyfd=%d\n",readyfd);
    if(readyfd<0){
      if(errno!=EINTR){   // interrupt is ok
        int flags;
        int bail=1;
        perror("select() returned error");
        for(handler=nb_medulla->handler;handler!=NULL;handler=handler->next){
          switch(handler->type){
            case 0: setP=&nb_medulla->readfds;   break;
            case 1: setP=&nb_medulla->writefds;  break;
            case 2: setP=&nb_medulla->exceptfds; break;
            default:
              printf("nbMedullaPulse: Logic error - invalid medulla file handler type=%d\n",handler->type);
              exit(NB_EXITCODE_FAIL);
            }
          flags=fcntl(handler->fildes,F_GETFL);
          if(flags==-1 && errno==EBADF){
             bail=0;
             handler->close=1;
             fprintf(stderr,"fd=%d is bad - removing from fd set, but we need to let the handler know\n",handler->fildes);
             }
          fprintf(stderr,"fd=%d FLAGS=%d\n",handler->fildes,flags);
          }
        if(bail){
          fprintf(stderr,"Terminating on error.\n");
          exit(NB_EXITCODE_FAIL);
          }
        }
      }
    else if(readyfd>0){             // invoke file handlers if any are set
      for(handler=nb_medulla->handler;handler!=NULL;handler=handler->next){
        switch(handler->type){
          case 0: setP=&nb_medulla->readfds;   break;
          case 1: setP=&nb_medulla->writefds;  break;
          case 2: setP=&nb_medulla->exceptfds; break;
          default:
            printf("nbMedullaPulse: Logic error - invalid medulla file handler type=%d\n",handler->type);
            exit(NB_EXITCODE_FAIL);
          }
        // call handler for any fildes set for response, and flag for removal on non-zero return code
        if(FD_ISSET(handler->fildes,setP) && (handler->handler)(handler->session)) handler->close=1;
        // 2010-01-02 eat - see if we can make write waits return when the peer closes the connection
        //if(handler->type==1 && FD_ISSET(handler->fildes,&nb_medulla->readfds) && (handler->handler)(handler->session)) handler->close=1;
        }
      }
    nbMedullaProcessHandler(0);   // check on processes and disable listeners if necessary
    // rebuild the sets in a separate pass because some may have been disabled by handlers in the previous pass
    nb_medulla->highfd=0;
    handlerP=&nb_medulla->handler;
    for(handler=*handlerP;handler!=NULL;handler=*handlerP){
      switch(handler->type){
        case 0: setP=&nb_medulla->readfds;   break;
        case 1: setP=&nb_medulla->writefds;  break;
        case 2: setP=&nb_medulla->exceptfds; break;
        default:
          printf("nbMedullaPulse: Logic error - invalid medulla file handler type=%d\n",handler->type);
          exit(NB_EXITCODE_FAIL);
        }
      if(handler->close){                // remove handlers flagged for removal
        FD_CLR((unsigned int)handler->fildes,setP);
        // 2010-01-02 eat - see if we can make write waits return when the peer closes the connection
        //if(handler->type==1) FD_CLR(handler->fildes,&nb_medulla->readfds);
        *handlerP=handler->next;
        handler->next=nb_medulla->handled;
        nb_medulla->handled=handler;
        }
      else{
        FD_SET(handler->fildes,setP);
        // 2010-01-02 eat - see if we can make write waits return when the peer closes the connection
        //if(handler->type==1) FD_SET(handler->fildes,&nb_medulla->readfds);
        if(handler->fildes>=nb_medulla->highfd) nb_medulla->highfd=handler->fildes+1;
        handlerP=&handler->next;
        }
      }
    if(!serve) nb_medulla->serving=0;
    }
  return(0);
  }
#endif

// Set flag to return from nbMedullaPulse()

int nbMedullaStop(void){
  nb_medulla->serving=0;
  //fprintf(stderr,"nbMedullaStop() called\n");
  //fflush(stderr);
  return(0);
  }

int nbMedullaServing(void){
  return(nb_medulla->serving);
  }

// Write messages to process stdin when the process is ready
// NOTE: When we convert the UNIX code to open a medulla file and
//       use the internal queue, we can colapse the Windows and UNIX
//       code into common code.
#if defined(WIN32)
int nbMedullaFileWriter(NB_MedullaFile *medfile,char *buffer,size_t size){
  BOOL fSuccess;
  int len,rc;
  //fprintf(stderr,"nbMedullaFileWriter() called\n");
  if(size>=sizeof(medfile->buffer)){
    fprintf(stderr,"nbMedullaFileWriter() - msg too large for buffer\n");
    }
  memcpy(medfile->buffer,buffer,size);
  fSuccess=WriteFile(medfile->file,medfile->buffer,size,&len,&medfile->olap);
  if(!fSuccess){
    switch(rc=GetLastError()){
      default:
        fprintf(stderr,"nbMedullaFileWriter() - WriteFile() failed errno=%d\n",rc);
      }
    }
  return(len);
  }
int nbMedullaFileWriterThreaded(NB_MedullaFile *medfile,char *buffer,size_t size){
  BOOL fSuccess;
  int len;
  fSuccess=WriteFile(medfile->file,buffer,size,&len,NULL);
  return(len);
  }
int nbMedullaProcessWriter(void *session){
  nbPROCESS process=(nbPROCESS)session;
  nbBUFFER buf;
  size_t size,len;

  //fprintf(stderr,"nbMedullaProcessWriter() called\n");
  // if we have no data in the queue, give the producer a chance to generate some
  if((buf=process->putpipe->queue->getbuf)==NULL){
    (process->producer)(process,process->pid,process->session); 
    // if still no data then disable the medulla listener
    if((buf=process->putpipe->queue->getbuf)==NULL){
      //printf("disabling the write listener\n");
      process->writerEnabled=0;
      return(1);  // request disable
      }
    }
  size=buf->free-buf->page;
  len=nbMedullaFileWriter(process->putpipe,buf->page,size);
  if(process->putpipe->queue->putbuf==process->putpipe->queue->getbuf){
    // don't we need to free the buffer here?
    process->putpipe->queue->putbuf=NULL;
    process->putpipe->queue->getbuf=NULL;
    }
  else process->putpipe->queue->getbuf=buf->next;
  nbMedullaBufferFree(buf);
  return(0);
  }
#else
int nbMedullaProcessWriter(void *session){
  nbPROCESS process=(nbPROCESS)session;
  //nbFILE fildes=process->putfile;
  nbBUFFER buf;
  size_t size,len;
  int eof;

  // if we have no data in the queue, give the producer a chance to generate some
  if(process->putQueue==NULL || (buf=process->putQueue->getbuf)==NULL){ // 2012-12-27 eat 0.8.13 - CID 751821
    eof=(process->producer)(process,process->pid,process->session); 
    // if still no data then disable the medulla listener
    if(process->putQueue==NULL || (buf=process->putQueue->getbuf)==NULL){
      //printf("disabling the write listener\n");
      process->writerEnabled=0;
      if(eof){    // close up the putfile if the producer returned eof
        fprintf(stderr,"nbMedullaProcessWriter - closing putfile\n");
        close(process->putfile);
        process->putfile=-1;
        if(process->putQueue!=NULL) process->putQueue=nbMedullaQueueClose(process->putQueue);
        }
      return(1);  // request disable
      }
    }
  size=buf->free-buf->page;
  len=write(process->putfile,buf->page,size);
  if(process->putQueue->putbuf==process->putQueue->getbuf){
    process->putQueue->putbuf=NULL;
    process->putQueue->getbuf=NULL;
    }
  else process->putQueue->getbuf=buf->next;
  nbMedullaBufferFree(buf);
  return(0);
  }
#endif

#if defined(WIN32)
DWORD WINAPI nbMedullaProcessWriterThreaded(void *session){
  //fprintf(stderr,"nbMedullaProcessWriterThreaded() called\n");
  return(0);
  }
#endif

// Read a file using blocking IO
// Return Code: 0 - success to end of file, 1 - error
#if defined(WIN32)
int nbMedullaFileReadBlocking(nbFILE file,void *session,int (*consumer)(void *session,char *msg)){
  BOOL fSuccess;
  int len,rc;
  char buffer[NB_BUFSIZE];
  nbQUEUE queue;

  queue=nbMedullaQueueOpen();
  fSuccess=ReadFile(file,buffer,sizeof(buffer),&len,NULL); 
  while(fSuccess){
    nbMedullaQueuePut(queue,buffer,len);
    len=nbMedullaQueueGet(queue,buffer,sizeof(buffer));
    while(len>0){
      //fprintf(stderr,"calling consumer:%s\n",medfile->buffer);
      rc=(consumer)(session,buffer);
      len=nbMedullaQueueGet(queue,buffer,sizeof(buffer));
      }
    fSuccess=ReadFile(file,buffer,sizeof(buffer),&len,NULL); 
    }
  switch(rc=GetLastError()){
    case ERROR_HANDLE_EOF:
      //fprintf(stderr,"nbMedullaFileReadBlocking() - eof\n");
      rc=0;
      break;
    case ERROR_BROKEN_PIPE:
      //fprintf(stderr,"nbMedullaFileReadBlocking() - broken pipe\n");
      rc=1;
      break;
    default:
      fprintf(stderr,"nbMedullaFileReadBlocking() - errorno=%d\n",rc);
      rc=1;
    }
  nbMedullaQueueClose(queue);
  return(rc);
  }

#else

int nbMedullaFileReadBlocking(nbFILE file,void *session,int (*consumer)(void *session,char *msg)){
  return(0);
  }

#endif



#if defined(WIN32) 

int nbMedullaProcessReadBlocking(nbPROCESS process){
  // 2007-12-26 eat - need to insert here a Windows version of first if block in unix version below
  if(process->logfile){
    nbMedullaFileReadBlocking(process->logfile,(void *)process,nbMedullaProcessFileLogger);
    CloseHandle(process->logfile);
    process->logfile=NULL;
    }
  if(process->getfile){
    nbMedullaFileReadBlocking(process->getfile,(void *)process,nbMedullaProcessFileConsumer);
    CloseHandle(process->getfile);
    process->getfile=NULL;
    }
  return(0);
  }

#else
 
int nbMedullaProcessReadBlocking(nbPROCESS process){
  fd_set fdset;
  char buffer[NB_BUFSIZE];
  int len,sent,rc;
  int readyfd;
  int maxfile;

  // if not currently blocking, switch to a blocking mode
  if(!(process->status&NB_MEDULLA_PROCESS_STATUS_BLOCKING)){
    // Our closing of the putfile here may turn out to be a problem.
    // It may be better to let the caller close it before calling this function.
    // By forcing it closed here, we avoid a deadly embrace where the child process
    // is waiting for EOF on stdin before closing stdout and stderr.
    if(process->putfile>=0){      // clean up output queue right now
      fprintf(stderr,"nbMedullaProcessWait - closing putfile\n");
      nbMedullaWaitDisable(1,process->putfile);
      close(process->putfile);
      process->putfile=-1;
      if(process->putQueue!=NULL) process->putQueue=nbMedullaQueueClose(process->putQueue);
      }
    if(process->logfile>0) nbMedullaWaitDisable(0,process->logfile);
    if(process->getfile>0) nbMedullaWaitDisable(0,process->getfile);
    process->status|=NB_MEDULLA_PROCESS_STATUS_BLOCKING;
    }
  // 2013-01-19 eat - reworked this for -:command option where the command didn't end causing stderr to block the process
  if(process->logfile<=0 && process->getfile<=0) return(0);
  while(1){  // read as long as we have data
    FD_ZERO(&fdset);  // this should be done once to a static variable
    maxfile=-1;
    if(process->logfile>=0){
      FD_SET(process->logfile,&fdset);
      maxfile=process->logfile;
      }
    if(process->getfile>=0){
      FD_SET(process->getfile,&fdset);
      if(process->getfile>maxfile) maxfile=process->getfile;
      }
    if(maxfile<0) return(0);
    readyfd=select(maxfile+1,&fdset,NULL,NULL,NULL);
    while(readyfd<0 && errno==EINTR) readyfd=select(maxfile+1,&fdset,NULL,NULL,NULL);
    if(readyfd<0){
      fprintf(stderr,"nbMedullaProcessReadBlocking: select error - %s\n",strerror(errno));
      return(1);
      }
    if(process->logfile>0 && FD_ISSET(process->logfile,&fdset)){
      len=read(process->logfile,buffer,sizeof(buffer));
      while(len==-1 && errno==EINTR) len=read(process->logfile,buffer,sizeof(buffer));
      if(len<=0){
        if(len<0) fprintf(stderr,"[%d] Error reading from process stderr\n",process->pid);
        //else fprintf(stderr,"[%d] End of file on stderr\n",process->pid);
        close(process->logfile);
        process->logfile=-1;
        if(process->logQueue!=NULL) process->logQueue=nbMedullaQueueClose(process->logQueue);
        if(process->getfile<0 && process->status&NB_MEDULLA_PROCESS_STATUS_ENDED)
          nbMedullaProcessClose(process);
        }
      else{
        sent=nbMedullaQueuePut(process->logQueue,buffer,len);
        len=nbMedullaQueueGet(process->logQueue,buffer,sizeof(buffer));
        while(len>=0){
          rc=(process->logger)(process,process->pid,process->session,buffer);
          len=nbMedullaQueueGet(process->logQueue,buffer,sizeof(buffer));
          }
        }
      }
    if(process->getfile>0 && FD_ISSET(process->getfile,&fdset)){
      len=read(process->getfile,buffer,sizeof(buffer));
      while(len==-1 && errno==EINTR) len=read(process->getfile,buffer,sizeof(buffer));
      if(len<=0){
        if(len<0) fprintf(stderr,"[%d] Error reading from process stdout\n",process->pid);
        //else fprintf(stderr,"[%d] End of file on stdout\n",process->pid);
        close(process->getfile);
        process->getfile=-1;
        if(process->getQueue!=NULL) process->getQueue=nbMedullaQueueClose(process->getQueue);
        if(process->logfile<0 && process->status&NB_MEDULLA_PROCESS_STATUS_ENDED)
          nbMedullaProcessClose(process);
        }
      else{
        sent=nbMedullaQueuePut(process->getQueue,buffer,len);
        len=nbMedullaQueueGet(process->getQueue,buffer,sizeof(buffer));
        while(len>=0){
          rc=(process->consumer)(process,process->pid,process->session,buffer);
          len=nbMedullaQueueGet(process->getQueue,buffer,sizeof(buffer));
          }
        }
      }
    }
  }

#endif

#if defined(WIN32)
int nbMedullaFileReader(NB_MedullaFile *medfile,void *session,int (*consumer)(void *session,char *msg)){
  BOOL fSuccess,eof=0;
  int have,len,rc;
  //fprintf(stderr,"nbMedullaFileReader() called\n");
  fSuccess = GetOverlappedResult(medfile->file,&medfile->olap,&medfile->len,FALSE);
  if(!fSuccess){
    switch(rc=GetLastError()){
      case ERROR_HANDLE_EOF:
        fprintf(stderr,"nbMedullaFileReader() - GetOverlappedResult returned EOF\n");
        eof=1;
        break;
      case ERROR_BROKEN_PIPE:
        eof=1;
        break;
      case ERROR_IO_INCOMPLETE:
        //fprintf(stderr,"nbMedullaFileReader() - IO incomplete\n"); //debug
        return(0);
        break;
      case ERROR_IO_PENDING:
        //fprintf(stderr,"nbMedullaFileReader() - IO pending\n"); //debug
        return(0);
        break;
      default:
        fprintf(stderr,"GetOverlappedResult failed errorno=%d\n",GetLastError()); 
      }
    }
  else if(medfile->len>0) have=nbMedullaQueuePut(medfile->queue,medfile->buffer,medfile->len); 
  // start another read
  fSuccess=ReadFile(medfile->file,medfile->buffer,NB_BUFSIZE,&medfile->len,&medfile->olap); 
  while(fSuccess){
    have+=nbMedullaQueuePut(medfile->queue,medfile->buffer,medfile->len);
    fSuccess=ReadFile(medfile->file,medfile->buffer,NB_BUFSIZE,&medfile->len,&medfile->olap); 
    }
  switch(rc=GetLastError()){
    case ERROR_HANDLE_EOF:
      fprintf(stderr,"nbMedullaFileReader() - eof\n");
      eof=1;
      break;
    case ERROR_IO_PENDING:
      //fprintf(stderr,"nbMedullaFileReader() - ReadRile IO Pending\n");
      break;
    case ERROR_BROKEN_PIPE:
      //fprintf(stderr,"nbMedullaFileReader() - broken pipe\n");
      eof=1;
      break;
    default:
      fprintf(stderr,"nbMedullaFileReader() - errorno=%d\n",rc);
      return(1);
    }
  len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
  while(len>0){
    //fprintf(stderr,"calling consumer:%s\n",medfile->buffer);
    rc=(consumer)(session,medfile->buffer);
    len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
    }
  return(eof);
  }
int nbMedullaFileReaderThreaded(NB_MedullaFile *medfile,void *session,int (*consumer)(void *session,char *msg)){
  BOOL fSuccess,eof=0;
  int have,len,rc;
  while(eof==0){
    fSuccess=ReadFile(medfile->file,medfile->buffer,NB_BUFSIZE,&medfile->len,NULL); 
    if(fSuccess) have=nbMedullaQueuePut(medfile->queue,medfile->buffer,medfile->len);
    else{
      switch(rc=GetLastError()){
        case ERROR_HANDLE_EOF:
          fprintf(stderr,"nbMedullaFileReader() - eof\n");
          eof=1;
          break;
        case ERROR_BROKEN_PIPE:
          eof=1;
          break;
        default:
          fprintf(stderr,"nbMedullaFileReader() - errorno=%d\n",rc);
          return(1);
        }
      }
    len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
    while(len>0){
      nbMedullaEventSchedule(session,medfile->buffer,consumer);
      len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
      }
    }
  return(eof);
  }
#else
int nbMedullaFileReader(NB_MedullaFile *medfile,void *session,int (*consumer)(void *session,char *msg)){
  //int eof=0;
  fd_set fdset;
  struct timeval tv;
  int readyfd=0,have,len,rc;

  tv.tv_usec=0;
  tv.tv_sec=0;
  //while(!eof){  // read as long as we have data
  while(1){  // read as long as we have data
    FD_ZERO(&fdset);  // this should be done once to a static variable
    FD_SET(medfile->file,&fdset);
    //fprintf(stderr,"nbMedullaFileReader calling select for fildes=%d\n",medfile->file);
    readyfd=select(medfile->file+1,&fdset,NULL,NULL,&tv);
    //fprintf(stderr,"nbMedullaFileReader back from select rc=%d\n",readyfd);
    if(readyfd<0){
      fprintf(stderr,"nbMedullaFileReader read errno=%d\n",errno);
      return(1);
      }
    if(!FD_ISSET(medfile->file,&fdset)) return(0);
    //fprintf(stderr,"nbMedullaFileReader going to read\n");
    medfile->len=read(medfile->file,medfile->buffer,sizeof(medfile->buffer));
    //fprintf(stderr,"len=%d\n",medfile->len);
    if(medfile->len<0){
      fprintf(stderr,"nbMedullaFileReader read errno=%d\n",errno);
      return(1);
      }
    else have=nbMedullaQueuePut(medfile->queue,medfile->buffer,medfile->len);
    //fprintf(stderr,"have=%d medfile len=%d\n",have,medfile->len);
    len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
    //fprintf(stderr,"queue get len=%d\n",len);
    while(len>=0){
      //fprintf(stderr,"calling consumer:%s\n",medfile->buffer);
      rc=(consumer)(session,medfile->buffer);
      len=nbMedullaQueueGet(medfile->queue,medfile->buffer,sizeof(medfile->buffer));
      }
    }
  //return(eof);  // 2012-12-15 eat - CID 751540
  }
#endif

// Send stdout lines to process consumer
int nbMedullaProcessFileConsumer(void *session,char *msg){
  nbPROCESS process=(nbPROCESS)session;
  (process->consumer)(process,process->pid,process->session,msg);
  return(0);
  }
#if defined(WIN32)
int nbMedullaProcessReaderClose(void *session,char *msg){
  nbPROCESS process=(nbPROCESS)session;
  nbMedullaFileClose(process->getpipe);
  process->getpipe=NULL;
//  process->getQueue=NULL;  // queue is closed by nbMedullaFileClose()
  if(process->status&NB_MEDULLA_PROCESS_STATUS_ENDED && process->logpipe==NULL)
    nbMedullaProcessClose(process);
  return(0);
  }
int WINAPI nbMedullaProcessReaderThreaded(void *session){
  nbPROCESS process=(nbPROCESS)session;
  nbMedullaFileReaderThreaded(process->getpipe,process,nbMedullaProcessFileConsumer);
  nbMedullaEventSchedule(process,NULL,nbMedullaProcessReaderClose); 
  return(0); 
  }
int nbMedullaProcessReader(void *session){
  nbPROCESS process=(nbPROCESS)session;
  int eof;
  eof=nbMedullaFileReader(process->getpipe,process,nbMedullaProcessFileConsumer);
  if(eof){ 
    nbMedullaFileClose(process->getpipe);
    process->getpipe=NULL;
//    process->getQueue=NULL;  // queue is closed by nbMedullaFileClose()
    if(process->status&NB_MEDULLA_PROCESS_STATUS_ENDED && process->logpipe==NULL)
      nbMedullaProcessClose(process);
    }
  return(eof);
  }
#else
int nbMedullaProcessReader(void *session){
  nbPROCESS process=(nbPROCESS)session;
  nbFILE fildes=process->getfile;
  char buffer[NB_BUFSIZE];
  int len,sent,rc;

  //fprintf(stderr,"[%d] nbMedullaProcessReader fildes=%d\n",process->pid,fildes);
  len=read(fildes,buffer,sizeof(buffer));
  while(len==-1 && errno==EINTR) len=read(fildes,buffer,sizeof(buffer));
  if(len<=0){
    if(len<0) fprintf(stderr,"[%d] Error reading from process stdout\n",process->pid);
    //else fprintf(stderr,"[%d] End of file on stdout\n",process->pid);
    close(fildes);
    process->getfile=-1;
    if(process->getQueue!=NULL) process->getQueue=nbMedullaQueueClose(process->getQueue);
    if(process->logfile<0 && process->status&NB_MEDULLA_PROCESS_STATUS_ENDED)
      nbMedullaProcessClose(process);
    return(1);  // end of file
    }
  sent=nbMedullaQueuePut(process->getQueue,buffer,len);
  len=nbMedullaQueueGet(process->getQueue,buffer,sizeof(buffer));
  while(len>=0){
    //fprintf(stderr,"calling consumer:%s\n",buffer);
    rc=(process->consumer)(process,process->pid,process->session,buffer);
    len=nbMedullaQueueGet(process->getQueue,buffer,sizeof(buffer));
    }
  //fprintf(stderr,"[%d] nbMedullaProcessReader fildes=%d returning\n",process->pid,fildes);
  return(0);
  }
#endif

// Send stderr lines to process logger

int nbMedullaProcessFileLogger(void *session,char *msg){
  nbPROCESS process=(nbPROCESS)session;
  (process->logger)(process,process->pid,process->session,msg);
  return(0);
  }
#if defined(WIN32)
int nbMedullaProcessLogger(void *session){
  nbPROCESS process=(nbPROCESS)session;
  int eof;
  eof=nbMedullaFileReader(process->logpipe,process,nbMedullaProcessFileLogger);
  if(eof){ 
    nbMedullaFileClose(process->logpipe);
    process->logpipe=NULL;
//    process->logQueue=NULL;  // queue is closed by nbMedullaFileClose()
    if(process->status&NB_MEDULLA_PROCESS_STATUS_ENDED && process->getpipe==NULL)
      nbMedullaProcessClose(process);
    }
  return(eof);
  }
#else
int nbMedullaProcessLogger(void *session){
  nbPROCESS process=(nbPROCESS)session;
  nbFILE fildes=process->logfile;
  char buffer[NB_BUFSIZE];
  int len,sent,rc;
  
  //fprintf(stderr,"nbMedullaProcessLogger(%d,...): called\n",fildes);
  len=read(fildes,buffer,sizeof(buffer));
  while(len==-1 && errno==EINTR) len=read(fildes,buffer,sizeof(buffer));
  if(len<=0){
    if(len<0) fprintf(stderr,"[%d] Error reading from process stderr\n",process->pid);
    //else fprintf(stderr,"[%d] End of file on stderr\n",process->pid);
    close(fildes);
    process->logfile=-1;
    if(process->logQueue!=NULL) process->logQueue=nbMedullaQueueClose(process->logQueue);
    if(process->getfile<0 && process->status&NB_MEDULLA_PROCESS_STATUS_ENDED)
      nbMedullaProcessClose(process);
    return(1);  // end of file
    }
  //fprintf(stderr,"buffer[%d]: %s\n",len,buffer);
  sent=nbMedullaQueuePut(process->logQueue,buffer,len);
  //fprintf(stderr,"sent [%d] to queue\n",sent);
  len=nbMedullaQueueGet(process->logQueue,buffer,sizeof(buffer));
  while(len>=0){
    //fprintf(stderr,"calling logger[%d]:%s\n",len,buffer);
    rc=(process->logger)(process,process->pid,process->session,buffer);
    len=nbMedullaQueueGet(process->logQueue,buffer,sizeof(buffer));
    }
  //fprintf(stderr,"queue msg len %d\n",len);
  return(0);
  }
#endif

// On windows we use threads to enable asynchronous I/O for stdin and stdout
// because we have no control over the way they are opened by the parent.  It
// looks like we will be able to use ReOpenFile() under Vista and Longhorn to
// set the FILE_FLAG_OVERLAPPED flag.  The would enable asynchronous I/O
// for stdin and stdout, eliminating the need for these threads.

#if defined(WIN32)
void nbMedullaThreadCreateW(void *session,DWORD (WINAPI *threadFunction)(void *session)){
  DWORD dwThreadId;
  HANDLE hThread;

  hThread = CreateThread(
    NULL,                        // no security attributes
    64000,                       // use default stack size
    threadFunction,              // thread function
    session,                     // argument to thread function
    0,                           // use default creation flags
    &dwThreadId);                // returns the thread identifier

  // Check the return value for success.

  if(hThread==NULL) fprintf(stderr,"nbMedullaThreadCreateW() CreateThread failed");
  else CloseHandle(hThread);
  }
#endif

// Enable process files
//   NOTE: This was writen for enabling the root process, but could be
//         modified to support an enable/disable pair of functions.  It
//         could also be used by nbMedullaProcessOpen() to avoid duplicate
//         code.  The enable/disable functions could be used by nb_mod_servant.c.
//
//   NOTE: On Windows, this function should only be called if the files where
//         openned with FILE_FLAG_OVERLAPPED or ReOpenFile() is supported
//         (Vista or Longhorn).  Otherwise, nbMedullaProcessEnableThreaded()
//         should be used instead.
int nbMedullaProcessEnable(
  nbPROCESS process,
  void *session,
  int (*producer)(nbPROCESS process,int pid,void *session),
  int (*consumer)(nbPROCESS process,int pid,void *session,char *msg)){

//  HANDLE newHandle;

  process->session=session;
  process->producer=producer;
  process->consumer=consumer;
  if(producer!=NULL){
#if defined(WIN32)
/* Requires Vista
    newHandle=ReOpenFile(
      process->putpipe->file, // original handle
      GENERIC_WRITE,            // access
      FILE_SHARE_WRITE,
      FILE_FLAG_OVERLAPPED);
*/
    if(process->putpipe->option==1)
      nbMedullaThreadCreateW(process,nbMedullaProcessWriterThreaded);
    else nbMedullaFileEnable(process->putpipe,process);
#else
    nbMedullaWaitEnable(1,process->putfile,process,nbMedullaProcessWriter);
#endif
    process->writerEnabled=1;
    }
  if(consumer!=NULL){
#if defined(WIN32)
/*
    newHandle=ReOpenFile(
      process->getpipe->file, // original handle
      GENERIC_READ,             // access
      FILE_SHARE_READ,
      FILE_FLAG_OVERLAPPED);
*/
    if(process->getpipe->option==1)
      nbMedullaThreadCreateW(process,nbMedullaProcessReaderThreaded);
    else nbMedullaFileEnable(process->getpipe,process);
#else
    nbMedullaWaitEnable(0,process->getfile,process,nbMedullaProcessReader);
#endif
    }
  return(0);
  }

// return code
//  -1 - syntax error
//   0 - not a file specification
//   1 - /dev/null ("!");
//   2 - logger ("|");
//   3 - write to generated file ("%")
//   4 - write to specified file
//   5 - append to specified file
//
int nbMedullaParseFileSpec(char filename[512],char **cursorP,char *msg,size_t msglen){
  char *cursor=*cursorP,*delim;
  int n,code=4;
  *filename=0;
  if(*cursor=='!'){
    cursor++;
    while(*cursor==' ') cursor++;
    *cursorP=cursor;
    return(1);
    }
  if(*cursor=='|'){
    cursor++;
    while(*cursor==' ') cursor++;
    *cursorP=cursor;
    return(2);
    }
  if(*cursor=='%'){
    cursor++;
    while(*cursor==' ') cursor++;
    *cursorP=cursor;
    return(3);
    }
  if(*cursor=='>'){
    cursor++;
    if(*cursor=='>'){
      cursor++;
      code=5;
      }
    while(*cursor==' ') cursor++; //skip blank bytes
    if(*cursor=='"'){  // if left " found, expect format "filename"
      cursor++;        // next byte
      delim=strchr(cursor,'"'); //search for right "
      if(delim==NULL){          //right " not found
        snprintf(msg,msglen,"Unbalanced quotes '\"' on output file name\n");
        return(-1); //return error
        }
      }
    else{                       //cursor it not a ", expect filename not in ""
      delim=strchr(cursor,' '); //mark end of filename at 1st blank
      if(delim==NULL) delim=strchr(cursor,0); //if blank not found, mark filename to end of string
      } 
    if((n=delim-cursor)<=0 || n>=512){ //dtl: added negative len check
      snprintf(msg,msglen,"Output file name too large for buffer\n");
      return(-1);
      } 
//  strncpy(filename,cursor,delim-cursor);
    else strncpy(filename,cursor,n); //dtl: moved to if block for Checker 
    *(filename+(delim-cursor))=0;    
    cursor=delim+1;
    while(*cursor==' ') cursor++;
    *cursorP=cursor;
    return(code); 
    }
  return(0);
  }

// Open a process 
//
// Command Syntax:
//
//   [|] (-|=) ["["user"]"] [OutSpec [ErrSpec]] [:] [($|@)[(*|"pgm")]] command
//
// Stdin:    [|]                  - request to create a stdin pipe
//
// Blocking: (-|=)                - "-" blocking, "=" not blocking
//
// User:     ["["user"]"]         - request to run under specified user  
//                                  Ignored with warning when not running as root
//
// OutSpec:  (!|"|"|%|>[>]"file") -
//
//                                  !        - /dev/null
//                                  |        - log file
//                                  %        - generated file in output directory
//                                  >"file"  - direct to file
//                                  >>"file" - append to file
//
// ErrSpec:  same as OutSpec      - stderr to a different destination
//                                  Defauls to OutSpec
//
// Interp:   [:]                  - stdout to interpreter
//
// Executable: [($|@)[(*|"pgm")]] - program to execute
//                                  $    - shell program (default host shell)
//                                  @    - program (default is /usr/local/bin/nb)
//                                  *    - substitute the program we are running
//                                  pgm  - substitute specified program
//
// Command:  command              - shell command or program arguments
//
//   When $ is used the command is passed as follows.
//
//      pgm "pgm" "-c" "command"
//
//   When @ is used, the command is parsed and passed
//   as individual parameters in a list.
//
//      pgm "pgm" "arg1" "arg2" "arg3"
//================================================
// 
// Note:  Since the calling program may not know how the command will parse out
//        the logfile and all handlers should be provided.  Any NULL values for
//        these parameters will override the corresponding request within the
//        command syntax.
//
// The only child option you should specify for now is NB_CHILD_TERM if you
// want to terminate the child when the parent process ends.

nbPROCESS nbMedullaProcessOpen(
  int  options,             // option flags for nbChild
                            //   NB_CHILD_TERM - SIGTERM when parent exits and use SIGHUP of SIGDLT
  char *cmd,                // command string
  char *logfile,            // output file name if needed (if % specified)
  void *session,            // handle for handler routines
  int (*closer)(nbPROCESS process,int pid,void *session),
  int (*producer)(nbPROCESS process,int pid,void *session),
  int (*consumer)(nbPROCESS process,int pid,void *session,char *msg),
  int (*logger)(nbPROCESS process,int pid,void *session,char *msg),
  char *msg,
  size_t msglen){

  nbPROCESS process;
//  int pid,cldpid,waitpid;
  nbFILE cldin,cldout,clderr;
  char *pgm=NULL;           // Program to execute
  char *cursor,*delim,mode,user[32],group[32],pgmname[sizeof(process->pgm)];
  char outfilename[512],*outfile=outfilename,errfilename[512],*errfile=errfilename;
  //int append=0,shell=1,outspec,errspec;
  int outspec,errspec;
  int uid=0,gid=0;
  struct passwd *pwd=NULL;
  struct group *grp=NULL;

  //fprintf(stderr,"nbMedullaProcessOpen %s\n",cmd);
  // limit the number of children we can start.
  if(nb_medulla_child_count>=nb_medulla_child_max){
    snprintf(msg,msglen,"Attempt to start more than %d children - request denied\n",nb_medulla_child_max); //2012-01-09 dtl use snprintf  // 2012-12-15 eat - CID 751589
    return(NULL);
    }
  // parse cmd into elements
  cursor=cmd;
  while(*cursor==' ') cursor++;
  // stdin
  if(*cursor!='|') producer=NULL;
  else{
    options|=NB_CHILD_TERM;
    cursor++;
    while(*cursor==' ') cursor++;
    }
  // mode
  if((mode=*cursor)!='-' && mode!='='){
    snprintf(msg,msglen,"Expecting '-' or '='\n");
    return(NULL);
    } 
  cursor++;
  while(*cursor==' ') cursor++;
  // user
  if(*cursor=='['){  // have user
    cursor++; 
    delim=cursor;
    while(*delim && *delim!=']' && *delim!='.') delim++;
    if(!*delim){
      snprintf(msg,msglen,"Expecting ']' as user delimiter\n");
      return(NULL);
      }
    if(delim-cursor>=sizeof(user)){
      snprintf(msg,msglen,"User is too long for buffer\n");
      return(NULL);
      }
    strncpy(user,cursor,delim-cursor);
    *(user+(delim-cursor))=0;
    cursor=delim+1;
    if((pwd=getpwnam(user))==NULL){
      snprintf(msg,msglen,"User %s not defined\n",user);
      return(NULL);
      }
    uid=pwd->pw_uid;
    gid=pwd->pw_gid;  // default to user's default group
    if(*delim=='.'){ // we have a group
      delim=cursor;
      while(*delim && *delim!=']') delim++;
      if(!*delim){
        snprintf(msg,msglen,"Expecting ']' as group delimiter\n");
        return(NULL);
        }
      if(delim-cursor>=sizeof(group)){
        snprintf(msg,msglen,"Group is too long for buffer\n");
        return(NULL);
        }
      strncpy(group,cursor,delim-cursor);
      *(group+(delim-cursor))=0;
      cursor=delim+1;
      if((grp=getgrnam(group))==NULL){
        snprintf(msg,msglen,"Group %s is not defined\n",group);
        return(NULL);
        }
      gid=grp->gr_gid;
      }
    while(*cursor==' ') cursor++;
    }
  else *user=0,*group=0;

  // output
  if((outspec=nbMedullaParseFileSpec(outfilename,&cursor,msg,msglen))<0) return(NULL);
  if((errspec=nbMedullaParseFileSpec(errfilename,&cursor,msg,msglen))<0) return(NULL);
  if(outspec==0){
    if(mode=='=') outspec=1;  // = defaults to /dev/null
    else outspec=2;           // - defaults to log file
    }
  else if(outspec<4){
    if(outspec==errspec) errspec=0; // ignore duplicate specification
    }
  else{
    logfile=outfilename;
    if(strcmp(logfile,errfilename)==0){ // duplicate file names
      if(outspec==errspec) errspec=0;   // ignore duplicate specification
      else if(errspec>3){
        snprintf(msg,msglen,"Conflicting output specifications");
        return(NULL);
        }
      }
    }
  if(outspec==3) outfile=logfile;      // if requested, point to generated file name
  else if(errspec==3) errfile=logfile; // we already made sure they are not both 3

  if(outspec>2 && *outfile==0) outspec=1;
  if(errspec>2 && *errfile==0) errspec=1;  

  if(outspec==2){
    options|=NB_CHILD_TERM;      // attached child if user wants to log
    if(logger==NULL) outspec=1;  // but don't log if we can't
    }
  else if(errspec==2){
    options|=NB_CHILD_TERM;      // attached child if user wants to log
    if(logger==NULL) errspec=1;  // but don't log if we can't
    }
  else logger=NULL;  // don't need logger

  if(*cursor==':'){  // direct stdout to consumer
    if(errspec==0){
      errspec=outspec;
      errfile=outfile;
      }
    if(consumer==NULL) outspec=1; // override if no consumer provided
    else outspec=0;               // 0 now means direct to consumer
    options|=NB_CHILD_TERM;
    cursor++;
    while(*cursor==' ') cursor++;
    }
  else consumer=NULL; // don't need consumer

  if(errspec==2 && logger==NULL) errspec=1; // override if no logger provided

  // program
  options|=NB_CHILD_SHELL;  // default to shell
  if(*cursor=='$' || *cursor=='@'){  // program specified
    if(*cursor=='@') options&=~NB_CHILD_SHELL;
    cursor++;
    if(*cursor==' ');  // default program
    else if(*cursor=='*' || *cursor=='@' || *cursor=='$'){
      cursor++;
      options|=NB_CHILD_CLONE;
      } 
    else{
      if(*cursor=='"'){
        cursor++;
        delim=strchr(cursor,'"');
        if(delim==NULL){
          snprintf(msg,msglen,"Unbalanced quotes on program file name\n");
          return(NULL);
          }
        }
      else{
        delim=strchr(cursor,' ');
        if(delim==NULL) delim=strchr(cursor,0);
        }
      if(delim-cursor>=sizeof(pgmname)){
        snprintf(msg,msglen,"Program file name longer than buffer\n");
        return(NULL);
        }
      strncpy(pgmname,cursor,delim-cursor);
      *(pgmname+(delim-cursor))=0;
      if(*delim==0) cursor=delim;
      else cursor=delim+1;
      pgm=pgmname;
      }
    }
  while(*cursor==' ') cursor++;
  if(cursor-cmd>=sizeof(process->prefix)){
    snprintf(msg,msglen,"Program file name longer than buffer\n");
    return(NULL);
    }
  if(strlen(cursor)>=sizeof(process->cmd)){
    snprintf(msg,msglen,"Command longer than buffer\n");
    return(NULL);
    }  
  if(logfile && strlen(logfile)>=sizeof(process->out)){  // 2012-12-27 eat 0.8.13 - CID 751719
    snprintf(msg,msglen,"Logfile longer than buffer\n");
    return(NULL);
    }  
  process=nbAlloc(sizeof(struct NB_MEDULLA_PROCESS));
  process->status=0;
  if(outspec==3 || errspec==3) process->status|=NB_MEDULLA_PROCESS_STATUS_GENFILE;
  *process->exittype=0;
  process->exitcode=0;
  process->options=options;
  process->closer=closer;
  process->producer=producer;
  process->consumer=consumer;
  process->logger=logger;
  process->session=session;
  process->writerEnabled=0;
  process->uid=uid;
  process->gid=gid;
  if(pgm) strcpy(process->pgm,pgm);
  else *process->pgm=0;
  strncpy(process->prefix,cmd,cursor-cmd);
  *(process->prefix+(cursor-cmd))=0;
  strcpy(process->cmd,cursor);
  if(logfile) strcpy(process->out,logfile);
  else *process->out=0;
  process->putpipe=NULL;
  process->getpipe=NULL;
  process->logpipe=NULL;
// the file and queue references can be folded into the file object pointed to by the pipe variables
#if defined(WIN32)
  process->putfile=NULL;
  process->getfile=NULL;
  process->logfile=NULL;
#else
  process->putfile=-1;
  process->getfile=-1;
  process->logfile=-1;
  process->putQueue=NULL;
  process->getQueue=NULL;
  process->logQueue=NULL;
#endif
  process->prior=nb_process->prior;  // insert at end of list
  process->next=nb_process;  

  // from here to there can be a start function that we'll need
  // to call again when the process dies.
  // nbMedullaProcessStart()

  // we only need to open an output file if we have one and the
  // either stdout or stderr is not directed to a handler

#if defined(WIN32)
  if(producer!=NULL) nbPipe(&process->putfile,&cldin);
  else cldin=CreateFile("nul",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  switch(outspec){
    case 0: nbPipe(&process->getfile,&cldout); break;
    case 1: cldout=CreateFile("nul",GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); break;
    case 2: nbPipe(&process->logfile,&cldout); break;
    case 5: cldout=CreateFile(outfile,FILE_APPEND_DATA,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); break;
    default: cldout=CreateFile(outfile,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    }
  if(cldout==INVALID_HANDLE_VALUE){
    snprintf(msg,msglen,"Unable to open child stdout\n");
    return(NULL);
    }
  switch(errspec){
    case 0: DuplicateHandle(GetCurrentProcess(),cldout,GetCurrentProcess(),&clderr,0,TRUE,DUPLICATE_SAME_ACCESS); break;
    case 1: clderr=CreateFile("nul",GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); break;
    case 2: nbPipe(&process->logfile,&clderr); break;
    case 5: clderr=CreateFile(outfile,FILE_APPEND_DATA,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); break;
    default: clderr=CreateFile(outfile,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    }
  if(clderr==INVALID_HANDLE_VALUE){
    snprintf(msg,msglen,"Unable to open child stderr\n");
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS));
    return(NULL);
    }

#else
  if(producer!=NULL) nbPipe(&process->putfile,&cldin);
  else cldin=open("/dev/null",O_RDONLY);
  if(cldin<0){
    snprintf(msg,msglen,"Unable to open child stdin\n");  // 2012-12-27 eat 0.8.13 - CID 751563
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS));
    return(NULL);
    }
  switch(outspec){
    case 0: nbPipe(&cldout,&process->getfile); break;
    case 1: cldout=open("/dev/null",O_WRONLY); break;
    case 2: nbPipe(&cldout,&process->logfile); break;
    case 5: cldout=open(outfile,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR); break;
    default: cldout=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
    }
  if(cldout<0){
    snprintf(msg,msglen,"Unable to open child stdout\n");
    close(cldin);   // 2012-12-18 eat - CID 751599
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS));
    return(NULL);
    }
  switch(errspec){
    case 0: clderr=dup(cldout); break;
    case 1: clderr=open("/dev/null",O_WRONLY); break;
    case 2: nbPipe(&clderr,&process->logfile); break;
    case 5: clderr=open(errfile,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR); break;
    default: clderr=open(errfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
    }
  if(clderr<0){
    snprintf(msg,msglen,"Unable to open child stderr\n");
    close(cldin);   // 2012-12-18 eat - CID 751600
    close(cldout);
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS));
    return(NULL);
    }
#endif

  process->child=nbChildOpen(process->options,process->uid,process->gid,process->pgm,process->cmd,cldin,cldout,clderr,msg,msglen);
  if(process->child==NULL){
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS));
    return(NULL);
    }
  process->pid=process->child->pid;
  process->status=NB_MEDULLA_PROCESS_STATUS_STARTED;
#if defined(WIN32)
  // Watch for process completion if running asynchronously
  // Otherwise, we expect the caller to issue a call to nbMedullaProcessWait()
  if(mode=='=') nbMedullaWaitEnable(process->child->handle,process,nbMedullaProcessHandler);
#endif
  // insert in the process list
  process->prior->next=process;
  nb_process->prior=process;
  nb_medulla_child_count++;

  if(mode=='-'){
    process->status|=NB_MEDULLA_PROCESS_STATUS_BLOCKING;
// We need to unify the Windows and Unix code better
// For now, we only need to open the queues for the Unix code
#if !defined(WIN32)
    if(consumer!=NULL) process->getQueue=nbMedullaQueueOpen();
    if(logger!=NULL) process->logQueue=nbMedullaQueueOpen();
#endif
    }
  else{
    // build queues and enable file listeners
    if(producer!=NULL){
#if defined(WIN32)
      process->putpipe=nbMedullaFileOpen(0,process->putfile,process,nbMedullaProcessWriter);
      nbMedullaFileEnable(process->putpipe,process);
#else
      nbMedullaWaitEnable(1,process->putfile,process,nbMedullaProcessWriter);   
      process->putQueue=nbMedullaQueueOpen();
#endif
      process->writerEnabled=1;
      //fprintf(stderr,"writerEnabled\n");
      }
    if(consumer!=NULL){
#if defined(WIN32)
      process->getpipe=nbMedullaFileOpen(0,process->getfile,process,nbMedullaProcessReader);
      nbMedullaFileEnable(process->getpipe,process);
#else
      nbMedullaWaitEnable(0,process->getfile,process,nbMedullaProcessReader);
      process->getQueue=nbMedullaQueueOpen();
#endif
      }
    if(logger!=NULL){
#if defined(WIN32)
      process->logpipe=nbMedullaFileOpen(0,process->logfile,process,nbMedullaProcessLogger);
      nbMedullaFileEnable(process->logpipe,process);
#else
      nbMedullaWaitEnable(0,process->logfile,process,nbMedullaProcessLogger);
      process->logQueue=nbMedullaQueueOpen();
#endif
      }
    }
  return(process);
  }

nbPROCESS nbMedullaProcessAdd(int pid,char *cmd){
  nbPROCESS process;
  process=nbAlloc(sizeof(struct NB_MEDULLA_PROCESS));
  process->pid=pid;
#if defined(WIN32)
  process->putfile=NULL;
  process->getfile=NULL;
#else
  process->putfile=-1;
  process->getfile=-1;
#endif
  strncpy(process->cmd,cmd,sizeof(process->cmd));
  if(strlen(cmd)>=sizeof(process->cmd)) *(process->cmd+sizeof(process->cmd)-1)=0;
  process->next=nb_process->next;    // insert in list
  process->prior=nb_process;
  nb_process->next=process;
  nb_medulla_child_count++;          // increment child count without checking child_max
  return(process);
  }

int nbMedullaProcessPid(nbPROCESS process){
  return(process->pid);
  }

int nbMedullaProcessStatus(nbPROCESS process){
  return(process->status);
  }

char *nbMedullaProcessCmd(nbPROCESS process){
  return(process->cmd);
  }

nbFILE nbMedullaProcessFile(nbPROCESS process,int option){
  if(option==NB_FILE_OUT) return(process->putfile);
  else if(option==NB_FILE_IN) return(process->getfile);
#if defined(WIN32)
  return(NULL);
#else
  return(-1);
#endif
  }

int nbMedullaProcessPut(nbPROCESS process,char *msg){
  size_t size;
  //fprintf(stderr,"nbMedullaProcessPut called: %s",msg);
#if defined(WIN32)
  size=nbMedullaQueuePut(process->putpipe->queue,msg,strlen(msg));
#else
  //if(process->putQueue) fprintf(stderr,"nbMedullaProcessPut have queue\n");
  //else fprintf(stderr,"nbMedullaProcessPut do not have queue\n");
  size=nbMedullaQueuePut(process->putQueue,msg,strlen(msg));
#endif
  // NOTE: We should move the writerEnabled flag into the Medulla file structure
  if(process->writerEnabled==0){
    //fprintf(stderr,"enabling the write listener for file %d\n",process->putfile);
#if defined(WIN32)
    nbMedullaFileEnable(process->putpipe,process);
//    nbMedullaWaitEnable(process->putpipe->olap.hEvent,process,process->putpipe->handler);
#else
    nbMedullaWaitEnable(1,process->putfile,process,nbMedullaProcessWriter);
#endif
    process->writerEnabled=1;
    }
  return(size);
  }

int nbMedullaProcessTerm(nbPROCESS process){
  int i=0;
#if defined(WIN32)
  //fprintf(stderr,"nbMedullaProcessTerm() called for process %d\n",process->pid);
  //fflush(stderr);
  if(!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,process->pid)){
    fprintf(stderr,"nbMedullaProcessTerm(): GenerateConsoleCtrlEvent failed errno=%d\n",GetLastError());
    i=1;
    }
#else
  // A return code of 3 from kill means the process is not found - we are ok with that for now
  if(process->options&NB_CHILD_SESSION){
    i=kill(-process->pid,SIGHUP);            // send SIGHUP to process group
    if(i<0){
      if(errno!=ESRCH) fprintf(stderr,"nbMedullaProcessTerm(): SIGHUP to %d session leader failed - %s\n",process->pid,strerror(errno));
      }
    else i=0;
    }
  else i=-1;
  if(i<0){
    i=kill(process->pid,SIGHUP);     // send SIGHUP to process
    if(i<0){
      if(errno!=ESRCH) fprintf(stderr,"nbMedullaProcessTerm(): SIGHUP to %d failed - %s\n",process->pid,strerror(errno));
      }
    else i=0;
    }
#endif
  return(i);
  }

// Close a Medulla process
//
//   Currently this function should only be called after the process ends and all
//   pipes have been closed.  We expect this to be the case when the nbMedullaProcessHander()
//   is used in combination with the nbMedullaProcessReader() and the nbMedullaProcessLogger().
//
//   If the REUSE flag is on in the status the process structure will not be released.
//
// Returns NULL if you want to use it to clear a process pointer.

nbPROCESS nbMedullaProcessClose(nbPROCESS process){
  //fprintf(stderr,"[%d] nbMedullaProcessClose() called\n",process->pid);
  if(!(process->status&NB_MEDULLA_PROCESS_STATUS_ENDED)){ 
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called while process is still running\n",process->pid);
    }
  nb_medulla->processHandler(process,process->child->pid,process->exittype,process->exitcode);
  process->child=nbChildClose(process->child);

  // Let the optional closer clean up a session related to the process that ended
  if(process->closer) (process->closer)(process,process->pid,process->session); 
#if defined(WIN32)
  if(process->putpipe!=NULL){
    //It is normal for a process to end while we still have an open pipe to stdin
    nbMedullaFileClose(process->putpipe);  // just close it
    process->putpipe=NULL;
#else
  if(process->putfile>=0){
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called with open stdin\n",process->pid);
    close(process->putfile);
    process->putfile=-1;
#endif
    }
#if defined(WIN32)
  if(process->getpipe!=NULL){
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called with open stdout\n",process->pid);
    nbMedullaFileClose(process->getpipe);
    process->getpipe=NULL;
#else
  if(process->getfile>=0){
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called with open stdout\n",process->pid);
    close(process->getfile);
    process->getfile=-1;
#endif
    }
#if defined(WIN32)
  if(process->logpipe!=NULL){
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called with open stderr\n",process->pid);
    nbMedullaFileClose(process->logpipe);
    process->logpipe=NULL;
#else
  if(process->logfile>=0){
    fprintf(stderr,"[%d] Logic Error: nbMedullaProcessClose() called with open stderr\n",process->pid);
    close(process->logfile);
    process->logfile=-1;
#endif
    }
#if !defined(WIN32)
  if(process->putQueue!=NULL) process->putQueue=nbMedullaQueueClose(process->putQueue);
  if(process->getQueue!=NULL) process->getQueue=nbMedullaQueueClose(process->getQueue);
  if(process->logQueue!=NULL) process->logQueue=nbMedullaQueueClose(process->logQueue);
#endif
  // check for reuse flag and don't destroy the process structure when set
  if(!(process->status&NB_MEDULLA_PROCESS_STATUS_REUSE)){ // release memory if not reused
    process->prior->next=process->next;
    process->next->prior=process->prior;
    nbFree(process,sizeof(struct NB_MEDULLA_PROCESS)); // put this back just checking
    }
  nb_medulla_child_count--;
  return(NULL);
  }


nbPROCESS nbMedullaProcessFind(int pid){
  nbPROCESS process;
  if(pid==0) return(nb_process);  // return root process when pid is zero
  for(process=nb_process->next;process!=nb_process && pid!=process->pid;process=process->next);
  if(pid==process->pid) return(process);
  return(NULL);
  } 

//=======================================================================================


nbQUEUE nbMedullaQueueOpen(void){
  nbQUEUE queue;
  //fprintf(stderr,"nbMedullaQueueOpen\n");
  queue=nbAlloc(sizeof(struct NB_MEDULLA_QUEUE));
  queue->getbuf=NULL;
  queue->putbuf=NULL;
  return(queue);
  }

nbQUEUE nbMedullaQueueClose(nbQUEUE queue){
  struct NB_MEDULLA_BUFFER *buf,*nextbuf;
  //fprintf(stderr,"nbMedullaQueueClose\n");
  for(buf=queue->getbuf;buf!=NULL;buf=nextbuf){
    nextbuf=buf->next; 
    nbMedullaBufferFree(buf);
    }
  nbFree(queue,sizeof(struct NB_MEDULLA_QUEUE));
  //fprintf(stderr,"nbMedullaQueueClose returning\n");
  return(NULL);
  }

// Put a message to a queue
//   -1 returned when too many buffers (not yet implemented)

int nbMedullaQueuePut(nbQUEUE queue,char *msg,size_t size){
  struct NB_MEDULLA_BUFFER *buf;
  size_t len,msgleft=size;

  //fprintf(stderr,"nbMedullaQueuePut: [%d]\n",size);  
  if((buf=queue->putbuf)==NULL){
    //fprintf(stderr,"nbMedullaQueuePut: putbuf is NULL\n");
    buf=nbMedullaBufferAlloc();
    if(queue->getbuf==NULL) queue->putbuf=buf;
    queue->getbuf=buf;
    }
  len=buf->end-buf->free;
  //fprintf(stderr,"nbMedullaQueuePut: len=%d, msgleft=%d\n",len,msgleft);
  while(len<msgleft){
    strncpy(buf->free,msg,len);
    msg+=len;
    msgleft-=len;
    buf->free=buf->end;
    if(buf->next==NULL){  // after testing we can remove this check
      buf->next=nbMedullaBufferAlloc();
      buf=buf->next;
      queue->putbuf=buf;
      }
    else{
      printf("something is crazy in nbMedullaQueuePut()\n");
      exit(NB_EXITCODE_FAIL);
      }
    len=buf->end-buf->free;
    //fprintf(stderr,"nbMedullaQueuePut: len=%d, msgleft=%d\n",len,msgleft);
    }
  strncpy(buf->free,msg,msgleft); 
  buf->free+=msgleft;
  return(size);
  }

// Get a message from a queue
//  -1  returned when no message available
//   n  returned as message length
//      When equal to size, the caller needs to
//      call again to get the rest of it.

int nbMedullaQueueGet(nbQUEUE queue,char *msg,size_t size){
  struct NB_MEDULLA_BUFFER *buf,*buftmp;
  char *delim;
  size_t len,fullsize=0,msgleft=size;
  
  
  if((buf=queue->getbuf)==NULL) return(-1);
  len=buf->free-buf->data;
  delim=memchr(buf->data,'\n',len);
  while(delim==NULL && len<=msgleft){
    //fprintf(stderr,"in the funny loop that will change buf->data\n");
    strncpy(msg,buf->data,len);
    fullsize+=len;
    msg+=len;
    msgleft-=len;
    if((buf=buf->next)==NULL) return(-1);
    len=buf->free-buf->data;
    delim=memchr(buf->data,'\n',len); 
    }
  // 2012-05-29 eat - modified to detect hopeless situation
  //if(delim==NULL) return(-1);
  if(delim==NULL){
    fprintf(stderr,"logic error in nbMedullaQueueGet - newline not found within size of the following return buffer\n");
    *(msg+size-1)=0;
    fprintf(stderr,"%s\n",msg);
    fprintf(stderr,"fatal error - terminating\n");
    exit(NB_EXITCODE_FAIL);
    }
  len=delim-buf->data;
  if(len>msgleft) len=msgleft;
  strncpy(msg,buf->data,len);
  buf->data+=len+1;
  fullsize+=len;
  if(fullsize<size){
    *(msg+len)=0;
#if defined(WIN32)
    if(*(msg+len-1)==13) *(msg+len-1)=0;
#endif
    }
  while(queue->getbuf!=buf){
    if(queue->getbuf==NULL || buf==NULL){
      fprintf(stderr,"logic error in nbMedullaQueueGet\n");
      exit(NB_EXITCODE_FAIL);
      }
    buftmp=queue->getbuf;
    queue->getbuf=queue->getbuf->next;
    nbMedullaBufferFree(buftmp);
    }
  if(buf->data>buf->free){
    printf("something is crazy in nbMedullaQueueGet()\n");
    exit(NB_EXITCODE_FAIL);
    }
  if(buf->data==buf->free){
    if(queue->getbuf==queue->putbuf) queue->putbuf=NULL;
    queue->getbuf=buf->next;
    nbMedullaBufferFree(buf);
    }
  //fprintf(stderr,"nbMedullaQueueGet returning\n");
  //fprintf(stderr,"nbMedullaQueueGet: %d [%s]\n",fullsize,msg);
  return(fullsize);
  }

//====================================================================

NB_MedullaFile *nbMedullaFileOpen(int option,nbFILE file,void *session,int (*handler)(void *medFile)){
  NB_MedullaFile *mfile;

  mfile=nbAlloc(sizeof(NB_MedullaFile));
  mfile->option=option;
#if defined(WIN32)
  memset(&mfile->olap,0,sizeof(OVERLAPPED));
  mfile->olap.hEvent=CreateEvent( 
    NULL,    // default security attribute 
    TRUE,    // manual-reset event 
    TRUE,    // initial state = signaled 
    NULL);   // unnamed event object 
#endif
  mfile->file=file;
  mfile->session=session;
  mfile->handler=handler;
  mfile->queue=nbMedullaQueueOpen();
  return(mfile); 
  }

int nbMedullaFileClose(NB_MedullaFile *mfile){
#if defined(WIN32)
  CloseHandle(mfile->file);
#else
  close(mfile->file);
#endif
  nbMedullaQueueClose(mfile->queue);
  nbFree(mfile,sizeof(NB_MedullaFile));
  return(0);
  }

#if defined(WIN32)
int nbMedullaFileEnable(NB_MedullaFile *mfile,void *session){
  nbMedullaWaitEnable(mfile->olap.hEvent,session,mfile->handler);
  return(0);
  }
// Insert UNIX version
#endif

#if defined(WIN32)
int nbMedullaFileDisable(NB_MedullaFile *mfile){
  nbMedullaWaitDisable(mfile->olap.hEvent);
  return(0);
  }
// Insert UNIX version
#endif

//====================================================================
// A medulla thread should not be confused with threads supported by
// the operating system.  This is just a simple mechanism that enables
// node modules to play well with others by taking turns using the
// CPU.  
//
// A thread handler is a "wait handler".  When threads are created,
// they are inserted into a circular list and invoked in sequence.
// A thread is deleted when the handler returns a non-zero value.
//
// FUTURE: Consider putting the guts of nbMedullaPulse into an initial
// thread, which would make sure the thread list is never empty and
// simplify the create and delete logic.
//   
void nbMedullaThreadCreate(NB_MEDULLA_WAIT_HANDLER handler,void *session){
  NB_Thread *thread;

  thread=nbAlloc(sizeof(NB_Thread));
  thread->handler=handler;
  thread->session=session;
  thread->next=nb_medulla->thread;
  thread->prior=nb_medulla->thread->prior;
  thread->next->prior=thread;
  thread->prior->next=thread;
  nb_medulla->thread_count++;
  }

// This should be the main program loop, but we are experimenting
// with calling it from nbMedullaPulse, so we only spin through one
// time for now.  To make it the main program loop, this can become
// nbMedullaPulse(), and the guts of nbMedullaPulse() can become
// a thread handler.

void nbMedullaThreadServe(){
  NB_Thread *thread=nb_medulla->thread,*prior;
  
  for(thread=thread->next;thread!=nb_medulla->thread;thread=thread->next){
    if(thread->handler(thread->session)){
      thread->prior->next=thread->next;
      thread->next->prior=thread->prior;
      prior=thread->prior;
      nbFree(thread,sizeof(NB_Thread));
      nb_medulla->thread_count--;
      thread=prior;
      }
    }
  }
