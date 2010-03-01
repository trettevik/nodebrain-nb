/*
* Copyright (C) 1998-2010 The Boeing Company
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
* File:     nbske.h 
* 
* Title:    Secret Key Encryption Header
*
* Function:
*
*   This header defines routines used for secret key encryption.
*
* See nbske.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Created to conform to new makefile
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_SKE_H_
#define _NB_SKE_H_

#include <stdlib.h>

typedef struct {                /* Key information structure */
  int   mode;                   /* >0 encrypt, <0 decrypt, =0 NOP */
  int   rounds;                 /* Number of rounds */
  unsigned int keySched[60];    /* (8+6+1)*4 word round key schedule */
  } skeKEY;

void skeSeedCipher(unsigned int cipher[4]);
void skeRandCipher(unsigned int cipher[4]);
void skeKeyData(unsigned int keySize,unsigned int keyData[]);
void skeKey(skeKEY *key,int keySize,unsigned int keyData[]);
void skeCipher(unsigned int *buffer,unsigned int blocks,unsigned int cipher[4],skeKEY *key);

#endif
