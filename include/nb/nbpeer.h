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
* File:     nbpeer.h
*
* Title:    Peer API Header
*
* Function:
*
*   This header defines routines that implement the NodeBrain Peer API. 
*
* See nbpeer.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010/01/07 eat 0.7.7  (original prototype) 
*=============================================================================
*/
#ifndef _NBPEER_H_
#define _NBPEER_H_       /* never again */

#define NB_PEER_BUFLEN 64*1024  // buffer length

typedef struct NB_PEER{
  int           flags;   // see NB_PEER_FLAG_* 
//  int           sd;      // socket we listen on - may preceed tls for now
//  nbTLSX        *tlsx;
  nbTLS         *tls;
  unsigned char *wbuf;
  unsigned char *wloc;
  unsigned char *rbuf;
  unsigned char *rloc;
  void          *handle;
  int  (*producer)(nbCELL context,struct NB_PEER *peer,void *handle);
  int  (*consumer)(nbCELL context,struct NB_PEER *peer,void *handle,void *data,int len);
  void (*shutdown)(nbCELL context,struct NB_PEER *peer,void *handle,int code);
  } nbPeer;

#define NB_PEER_FLAG_WRITE_WAIT  1
#define NB_PEER_FLAG_READ_WAIT   2
#define NB_PEER_FLAG_WRITE_ERROR 4

// API

extern int peerTrace;          // debugging trace flag for TLS routines

extern nbPeer *nbPeerConstruct(nbCELL context,int client,char *uriName,char *uri,nbCELL tlsContext,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));

extern void nbPeerModify(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));

extern int nbPeerListen(nbCELL context,nbPeer *peer); 

extern int nbPeerConnect(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));

extern int nbPeerSend(nbCELL context,nbPeer *peer,void *data,uint16_t len);

extern int nbPeerShutdown(nbCELL context,nbPeer *peer,int code);

extern int nbPeerDestroy(nbCELL context,nbPeer *peer);

#endif
