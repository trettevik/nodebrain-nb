/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbbrain.h
*
* Title:    Brain Structure Header
*
* Function:
*
*   This header defines the structure representing declared brains, and
*   defines related functions.  This structure is not yet defined as a
*   NodeBrain object.
*
* See nbbrain.c for more information.
*=============================================================================
* Change History:
*
*    Date     Name/Change
* ---------- -----------------------------------------------------------------
* 2002-10-06 Ed Trettevik (code moved here in version 0.4.1 A6)
* 2002-12-09 eat - version 0.4.3 B3
*            1) added queue management parameters (qsec,qfsize,qsize)
* 2003-10-06 eat 0.5.5 Added session handle to BRAIN structure.
*            Intended for brains that hold a session open.
* 2008-03-07 eat 0.6.9 Added myId and myIdentity to BRAIN.
* 2009-04-19 eat 0.7.5 Added context to support copy to queue
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_BRAIN_H_
#define _NB_BRAIN_H_

#include "nbprotokey.h"

struct BRAIN {
  struct NB_OBJECT object;     /* object header */
  int    version;              // version number 0 - pre-0.6.8, 1 - 0.6.8 and later
  struct TERM *context;        // context term - temp way to verify brain relationship to peer client
  char   *myId;                /* brain's identity name */
  NB_PeerKey *myIdentity;      /* brain's identity structure */
  char   *id;                  /* brain's identity name */
  NB_PeerKey *identity;        /* brain's identity structure */
  char   *hostname;            /* system host name */
  char   *ipaddr;              /* ip address for communication */
  unsigned short  port;        /* port number brain listens on */
  unsigned char   spec;        /* specification number */
  struct STRING *dir;          /* queue directory name */
  int    qsec;                 /* interval seconds */
  size_t qfsize;               /* interval file size limit */
  size_t qsize;                /* queue size limit */
  struct STRING *skullTarget;  /* brain name used by peer's skull */
  struct NBP_SESSION *session; /* active session */
  int    dsec;                 /* seconds of inactivity before disconnect */
  int    rsec;                 /* seconds after error before reconnect */
  };

struct PERMISSION{               /* permission grant or deny entry */
  struct PERMISSION *next;       /* next permission */
  nbIDENTITY         ident;      /* identity granted permission */
  struct REGEXP     *regexp;     /* regular expression for granted command */
  };

extern NB_Term *brainC;        /* brain context */
extern struct HASH *brainH;        /* brain hash */

/* methods */

void printBrain(struct BRAIN *brain);
void destroyBrain(struct BRAIN *brain);

NB_Term *getBrainTerm(char *ident);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct BRAIN *nbBrainNew(int version,char *cursor);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern NB_Term *nbBrainMakeTerm(struct TERM *context,struct BRAIN *brain);

#endif
