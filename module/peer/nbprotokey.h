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
