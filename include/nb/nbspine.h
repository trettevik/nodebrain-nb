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
* File:     nbspine.h
*
* Title:    Spinal API Layer
*
* Function:
*
*   This header defines functions that provide a platform independent API to
*   system services.
*
*   These functions must have no dependence on a NodeBrain environment.  They
*   must compile and link without any other NodeBrain headers and without the
*   NodeBrain Library (libnb.a).
*
* See nbsys.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/10/10 Ed Trettevik (introduced in 0.6.3)
* 2006/01/24 Converting to nbFILE type to reduce Windows variation
*            nbFILE is HANDLE on Windows and int on Linux/Unix
*=============================================================================
*/
#ifndef _NB_SPINE_H_
#define	_NB_SPINE_H_	 // only once

#include <nb/nbstd.h>

#if defined(WIN32)
typedef HANDLE nbFILE;
#else
typedef int nbFILE;
#endif


// nbChildOpen() options
#define NB_CHILD_SHELL       1  // Program is shell program
#define NB_CHILD_TERM        2  // Terminate with parent
#define NB_CHILD_SESSION     4  // Make leader of new process group
#define NB_CHILD_NOCHLD      8  // SIG_IGN SIGCHLD else SIGDFT
#define NB_CHILD_NOHUP      16  // SIG_IGN SIGHUP  else SIGDFT
#define NB_CHILD_NOTERM     32  // SIG_IGN SIGTERM else SIGDFT
#define NB_CHILD_NOCLOSE    64  // Don't close open files above 0,1,2 (stderr)
#define NB_CHILD_SHOWCLOSE 128  // Display message when closing file above 2 that don't have close-on-exec bit set
#define NB_CHILD_CLONE     256  // If program is unspecified, execute a copy of the current program

typedef struct NB_SPINE_CHILD{
#if defined(WIN32)
  HANDLE handle;
#endif
  int    pid;
  } NB_Child, *nbCHILD;

// Functions

void nbFileClose(nbFILE);

#if defined(WIN32)
int nbPipeWin(nbFILE *writePipe,nbFILE *readPipe);
#endif
int nbPipe(nbFILE *writePipe,nbFILE *readPipe);


nbCHILD nbChildOpen(int options,int uid,int gid,char *pgm,char *parms,nbFILE cldin,nbFILE cldout,nbFILE clderr,char *msg,size_t msglen);

nbCHILD nbChildClose(nbCHILD child);

#endif
