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
