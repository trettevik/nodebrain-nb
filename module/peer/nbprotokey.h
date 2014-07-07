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
* File:     nbprotokey.h 
* 
* Title:    NodeBrain Protocol Identity Header
*
* Function:
*
*   This header defines routines for managing NBP identity keys.
*
* See nbprotokey.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2008/03/24 eat 0.7.0  Created to transition key management to nb_mod_peer.c
*=============================================================================
*/
#ifndef _NB_PROTOKEY_H_
#define _NB_PROTOKEY_H_

#include "nbvli.h"

typedef struct NB_NBP_KEY{
  NB_TreeNode node;
  nbIDENTITY       identity; /* identity */
  vli1024 modulus;           /* encryption modulus */
  vli1024 exponent;          /* encryption exponent */
  vli1024 private;           /* private decryption exponent */
  unsigned long user;        /* user number */
  unsigned char authority;   /* basic authority */
  } NB_PeerKey;

extern NB_TreeNode *peerKeyTree;
//extern NB_Term *peerKeyC;
extern NB_PeerKey *nb_DefaultPeerKey;

/* functions */
NB_PeerKey *nbpNewPeerKey(char *name,char *key);
NB_PeerKey *nbpGetPeerKey(char *name);
void nbpLoadKeys();

#endif
