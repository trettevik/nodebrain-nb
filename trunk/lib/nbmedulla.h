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
* File:     nbmedulla.h 
* 
* Title:    NodeBrain Medulla Header
*
* Function:
*
*   This header defines NodeBrain Medulla API routines.
*
* See nbmedulla.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/12/10 Ed Trettevik (introduced in 0.6.4)
* 2007/05/04 eat 0.6.7 - increased command buffer size
* 2008/09/30 eat 0.7.1 - included thread pointer in medulla structure
*=============================================================================
*/
#ifndef _NB_MEDULLA_H_
#define	_NB_MEDULLA_H_	 // only once

#include "nbspine.h"         // Medulla API uses the Spinal API

#if defined(NB_INTERNAL)

typedef struct NB_MEDULLA_PROCESS{
  struct NB_MEDULLA_PROCESS *next;   // next entry in double linked list
  struct NB_MEDULLA_PROCESS *prior;  // prior entry in double linked list
  int    status;             // status - see NB_MEDULLA_PROCESS_STATUS_*
  char   exittype[8];        // exit type (Exit|Kill|Stop)
  int    exitcode;           // exit code
  char   prefix[256];        // command prefix
  int    options;            // child option flags - see nbChildOpen()
  int    uid;                // su user id (if non-zero)
  int    gid;                // sg group id (if uid non-zero)
  char   pgm[256];           // child program
  char   cmd[NB_BUFSIZE];    // child command - truncated if necessary
  char   out[256];           // output file name

  nbCHILD child;             // child information (may move some process info to it)
  int    pid;                // child process id
  void   *session;
  int    writerEnabled;      // 1 if writer enabled

  int    (*closer)(struct NB_MEDULLA_PROCESS *process,int pid,void *session);
  int    (*producer)(struct NB_MEDULLA_PROCESS *process,int pid,void *session);
  int    (*consumer)(struct NB_MEDULLA_PROCESS *process,int pid,void *session,char *msg);
  int    (*logger)(struct NB_MEDULLA_PROCESS *process,int pid,void *session,char *msg);
  struct NB_MEDULLA_FILE *putpipe; // child's stdin
  struct NB_MEDULLA_FILE *getpipe; // child's stdout
  struct NB_MEDULLA_FILE *logpipe; // child's stderr
//#if !defined(WIN32)
  nbFILE putfile;            // child's stdin
  nbFILE getfile;            // child's stdout
  nbFILE logfile;            // child's stderr
//#endif
// Once we get the Windows code working, we should move the queues into the
// NB_MEDULLA_FILE structure and modify the UNIX code to use the NB_MEDULLA_FILE
// structure.  We can then redefine nbFILE to be a pointer to NB_MEDULLA_FILE
#if !defined(WIN32)
  struct NB_MEDULLA_QUEUE *putQueue;
  struct NB_MEDULLA_QUEUE *getQueue;
  struct NB_MEDULLA_QUEUE *logQueue;
#endif
  } NB_Process,*nbPROCESS;

extern nbPROCESS nb_process;

#define NB_MEDULLA_PROCESS_TERM   1  // terminate on exit
#define NB_MEDULLA_PROCESS_EXISTS 4  // Process is already running

#define NB_FILE_OUT 1        // output file
#define NB_FILE_IN  2        // input file

#if defined(WIN32)
#define NB_MEDULLA_WAIT_OBJECTS 256  // number of object array elements
#endif 
typedef int (*NB_MEDULLA_WAIT_HANDLER)(void *session);

// Medulla thread structure
typedef struct NB_MEDULLA_THREAD{
  struct NB_MEDULLA_THREAD *next;
  struct NB_MEDULLA_THREAD *prior;
  NB_MEDULLA_WAIT_HANDLER handler;
  void *session;
  } NB_Thread;

typedef struct NB_MEDULLA{
  void   *session;
  int    (*scheduler)(void *session);
  int    (*processHandler)(struct NB_MEDULLA_PROCESS *process,int pid,char *status,int code);
  int    serving;
  int    service;    // running as windows service
#if defined(WIN32)
  int    waitCount;       // number of objects enabled
  HANDLE waitObject[NB_MEDULLA_WAIT_OBJECTS];
  void   *waitSession[NB_MEDULLA_WAIT_OBJECTS];
  NB_MEDULLA_WAIT_HANDLER waitHandler[NB_MEDULLA_WAIT_OBJECTS];
#else
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  int    highfd;                       // highest fildes in list
  struct NB_MEDULLA_WAIT *handler;
  struct NB_MEDULLA_WAIT *handled;  // free handler structures
#endif
  NB_Thread *thread;
  int    thread_count;              // number of threads
  } NB_Medulla,*nbMEDULLA;

extern nbMEDULLA nb_medulla;

// File Structure
//
// Currently the file structure is only used on Windows, but will be used on
// UNIX in the future with modifications
// 

typedef struct NB_MEDULLA_FILE{
  int option;                     // 0 - overlapped, 1 - threaded
#if defined(WIN32)
  OVERLAPPED olap;                // used for overlapped asynchronous I/0
#endif
  int len;
  char buffer[NB_BUFSIZE];
  struct NB_MEDULLA_QUEUE *queue; // move from process to here
  nbFILE file;
  void *session;
  int (*handler)(void *medfile);  // handler gets a pointer to this structure
  } NB_MedullaFile;

//typedef struct NB_MEDULLA_FILE NB_MedullaFile;

// Wait Structure
//
// Currently the wait structure is only used on Linux/UNIX
// An array is used on Windows
// In both cases, waits are managed using nbMedullaWaitEnable()/Disable()
// On UNIX we wait for a file to be ready for I/O.
// On Windows we wait for event completion, which may be I/O completion

struct NB_MEDULLA_WAIT{
  struct NB_MEDULLA_WAIT *next;
  int close;    // 1 means we need to close the file and remove listener 
  int type;
  nbFILE fildes;
  void *session;
  //int (*handler)(nbFILE fildes,void *session);
  NB_MEDULLA_WAIT_HANDLER handler;
  };

typedef struct NB_MEDULLA_QUEUE{
  struct NB_MEDULLA_BUFFER *getbuf;
  struct NB_MEDULLA_BUFFER *putbuf;
  } NB_Queue,*nbQUEUE;

//typedef struct NB_MEDULLA_QUEUE NB_Queue;
//typedef NB_Queue *nbQUEUE;

struct NB_MEDULLA_BUFFER{
  struct NB_MEDULLA_BUFFER *next;
  char *page;     // 4k buffer [4096]
  char *data;
  char *free;
  char *end;
  };

typedef struct NB_MEDULLA_BUFFER NB_Buffer;
typedef NB_Buffer *nbBUFFER;

// General functions

int nbMedullaOpen(void *session,int (*scheduler)(void *session),int (*processHandler)(nbPROCESS process,int pid,char *exittype,int exitcode));
int nbMedullaPulse(int serve);
int nbMedullaStop();
int nbMedullaServing();
int nbMedullaClose();

// Object/File functions

#if defined(WIN32)
int nbMedullaWaitEnable(HANDLE handle,void *session,NB_MEDULLA_WAIT_HANDLER handler);
int nbMedullaWaitDisable(HANDLE object);
#else
int nbMedullaWaitEnable(int type,nbFILE fildes,void *session,NB_MEDULLA_WAIT_HANDLER handler);
int nbMedullaWaitDisable(int type,nbFILE fildes);
#endif

// Process functions

#if defined(WIN32)
_declspec (dllexport)
#endif               
extern nbPROCESS nbMedullaProcessOpen(int options,char *cmd,char *logfile,void *session,
  int (*closer)(nbPROCESS process,int pid,void *session),
  int (*producer)(nbPROCESS process,int pid,void *session),
  int (*consumer)(nbPROCESS process,int pid,void *session,char *msg),
  int (*logger)(nbPROCESS process,int pid,void *session,char *msg),
  char *msgbuf);

//2009-02-13 eat - dup
//nbPROCESS nbMedullaProcessAdd(int pid,char *cmd);
//nbPROCESS nbMedullaProcessFind(int pid);
int nbMedullaProcessEnable(
  nbPROCESS process,
  void *session,
  int (*producer)(nbPROCESS process,int pid,void *session),
  int (*consumer)(nbPROCESS process,int pid,void *session,char *msg));

#if defined(WIN32)
_declspec (dllexport)
#endif               
extern int nbMedullaProcessPut(nbPROCESS process,char *buffer);

#if defined(WIN32)
_declspec (dllexport)
#endif               
extern int nbMedullaProcessPid(nbPROCESS process);
nbFILE nbMedullaProcessFile(nbPROCESS process,int option);
char *nbMedullaProcessCmd(nbPROCESS process);
//2009-02-13 eat dup
//int nbMedullaProcessTerm(nbPROCESS process);
//nbPROCESS nbMedullaProcessClose(nbPROCESS process);
void nbMedullaProcessLimit(int limit);
int nbMedullaProcessReadBlocking(nbPROCESS process);
int nbMedullaProcessWait(nbPROCESS process);

#if defined(WIN32)
int nbMedullaProcessHandler(nbPROCESS process);
#else
int nbMedullaProcessHandler(int wait);
#endif

// Internal - don't need to be in header for API
//struct NB_MEDULLA_BUFFER *nbMedullaBufferAlloc();
//void nbMedullaBufferFree(struct NB_MEDULLA_BUFFER *buf);

// Queue functions

nbQUEUE nbMedullaQueueOpen();
int nbMedullaQueuePut(nbQUEUE queue,char *msg,size_t size);
int nbMedullaQueueGet(nbQUEUE queue,char *msg,size_t size);
nbQUEUE nbMedullaQueueClose();

NB_MedullaFile *nbMedullaFileOpen(int option,nbFILE file,void *session,int (*handler)(void *medFile));
int nbMedullaFileReader(NB_MedullaFile *medfile,void *session,int (*consumer)(void *session,char *msg));
int nbMedullaFileClose(NB_MedullaFile *medfile);
#if defined(WIN32)
int nbMedullaFileEnable(NB_MedullaFile *medfile,void *session);
int nbMedullaFileDisable(NB_MedullaFile *medfile);

#endif
extern void nbMedullaThreadCreate(NB_MEDULLA_WAIT_HANDLER handler,void *session);

#else  // !NB_INTERNAL (external interface only)

typedef void*  nbPROCESS;  // void the API process pointer

#endif // !NB_INTERNAL

//***********************
// External API

// Process status flags
#define NB_MEDULLA_PROCESS_STATUS_STARTED   1  // running
#define NB_MEDULLA_PROCESS_STATUS_ENDED     2  // ran and ended
#define NB_MEDULLA_PROCESS_STATUS_REUSE     4  // retain for restart
#define NB_MEDULLA_PROCESS_STATUS_BLOCKING  8  // block until process completes
#define NB_MEDULLA_PROCESS_STATUS_GENFILE  16  // Using generated output file

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbPROCESS nbMedullaProcessOpen(int options,char *cmd,char *log,void *session,
  int (*closer)(nbPROCESS process,int pid,void *session),
  int (*producer)(nbPROCESS process,int pid,void *session),
  int (*consumer)(nbPROCESS process,int pid,void *session,char *msg),
  int (*logger)(nbPROCESS process,int pid,void *session,char *msg),
  char *msgbuf);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbMedullaProcessPid(nbPROCESS process);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbMedullaProcessStatus(nbPROCESS process);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbPROCESS nbMedullaProcessAdd(int pid,char *cmd);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbPROCESS nbMedullaProcessFind(int pid);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbMedullaProcessPut(nbPROCESS process,char *buffer);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbMedullaProcessTerm(nbPROCESS process);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbPROCESS nbMedullaProcessClose(nbPROCESS process);

#endif
