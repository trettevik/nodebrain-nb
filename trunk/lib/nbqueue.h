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
* File:     nbqueue.h
*
* Title:    NodeBrain Queue Header (prototype)
*
* Function:
*
*   This header defines routines that manage NodeBrain queues.
*
* See nbqueue.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/10/02 Ed Trettevik (original prototype introduced in 0.4.1 A5)
* 2002/12/07 eat - version 0.4.3 B3
*            1) included queue header file format
* 2003/01/13 eat - version 0.4.4 B1
*            1) simplified header record
* 2003/02/24 eat - version 0.5.0 
*            1) cleaned up a bit to reduce compiler warning messages
* 2009/02/14 eat 0.7.5 exported functions and made part of the nb library
*=============================================================================
*/
#ifndef _NB_QUEUE_H_
#define _NB_QUEUE_H_

#if defined(NB_INTERNAL)

extern char quedir[100];         /* queue directory  - <brain>.nbq */

#endif // NB_INTERNAL

#if defined(WIN32)
typedef HANDLE * NBQFILE;
#define NBQFILE_ERROR NULL
#else
typedef int NBQFILE; 
#define NBQFILE_ERROR -1
#endif

struct NBQ_HEADER{            /* 00000000000.000000.Q file format */
  char   version;             /* Version number: '3' */
  char   comma;               /* ',' */
  char   time[11];            /* File time: sssssssssss */
  char   dot;                 /* '.' */
  char   count[6];            /* File count: cccccc */
  char   nl;                  /* '\n' */
  };

/* lock options */
#define NBQ_UNLK 0            /* Unlock */
#define NBQ_TEST 1            /* Test for lock */
#define NBQ_WAIT 2            /* Wait for lock */

/* lock types */
#define NBQ_CONSUMER 1        /* Serialize consumers */
#define NBQ_PRODUCER 2        /* Serialize producers */

/* queue file naming scheme */
#define NBQ_INTERVAL 0
#define NBQ_UNIQUE   1
#define NBQ_NEXT     2

struct NBQ_HANDLE{
  nbCELL context;             // context for processing queue
  nbCELL pollSynapse;         // synapse for polling queue
  nbCELL yieldSynapse;        // synapse for yielding to other events
  char qname[256];            /* queue name including brain name */
  struct NBQ_ENTRY *entry;    /* queue object list */
  struct IDENTITY *identity;  /* depricated - use identity in entries */
  char   filename[512];       /* complete file name */
#if defined(WIN32)
  HANDLE *qfile;
  HANDLE *file;
  DWORD  markPos;
  DWORD  readPos;
  DWORD  eof;
#else
  int    qfile;
  int    file;
  long   markPos;
  long   readPos;
  long   eof;
#endif
  char   buffer[NB_BUFSIZE];
  char   *bufend;
  char   *cursor;
  };

struct NBQ_ENTRY{
  struct NBQ_ENTRY *next;     /* pointer to next entry */
  struct IDENTITY  *identity; /* originating identity */
  struct NB_TERM   *context;  /* future */
  char   type;                /* see NBQ_TYPE_? */
  char   filename[255];       /* file name */
  };

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbQueueGetNewFileName(char *qname,char *directory,int option,char type);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbQueueCommit();

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbQueueLock();

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbQueueGetFile(char *filename,char *dirname,char *identityName,int qsec,int option,unsigned char type);



#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbQueueRead(struct NBQ_HANDLE *qHandle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct NBQ_HANDLE *nbQueueOpenDir(char *dirname,char *siName,int mode);

#if defined(WIN32)
_declspec (dllexport)
extern HANDLE *nbQueueOpenFileName(char *filename,int option,int type);
#else
extern int nbQueueOpenFileName(char *filename,int option,int type);
#endif
#if defined(WIN32)
_declspec (dllexport)
extern long nbQueueSeekFile(HANDLE *file,int offset);
#else
extern long nbQueueSeekFile(int file,int offset);
#endif
#if defined(WIN32)
_declspec (dllexport)
extern size_t nbQueueWriteFile(HANDLE *file,char *buffer,size_t size);
#else
extern size_t nbQueueWriteFile(int file,char *buffer,size_t size);
#endif
#if defined(WIN32)
_declspec (dllexport)
extern void nbQueueCloseFile(HANDLE *file);
#else
extern void nbQueueCloseFile(int file);
#endif

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbQueueOpenFile(struct NBQ_HANDLE *qHandle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbQueueCloseDir(struct NBQ_HANDLE *qHandle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbQueueProcess(nbCELL context,char *dirname,nbCELL synapse);

#endif

