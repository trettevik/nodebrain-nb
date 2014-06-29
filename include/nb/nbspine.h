/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
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
