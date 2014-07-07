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
* File:     nbvli.h 
* 
* Title:    Very Large Integer Header
*
* Function:
*
*   This header defines large integer routines used for public key encryption.
*
* See nbvli.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Created to conform to new makefile
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_VLI_H_
#define _NB_VLI_H_

#include <nb/nbstd.h>

/*
*  Define the vli word size and data types with various numbers of bits.
*/

//typedef unsigned short vliWord;  /* word size is 16 bits */
typedef uint16_t vliWord;  /* word size is 16 bits */

typedef vliWord *vli;            /* Pointer to vli */

typedef vliWord vli0[1];         /* Arrays for various numbers of bits */
typedef vliWord vli8[2];
typedef vliWord vli16[2];
typedef vliWord vli32[3];
typedef vliWord vli64[5];
typedef vliWord vli128[9];
typedef vliWord vli256[17];
typedef vliWord vli512[33];
typedef vliWord vli1024[65];
typedef vliWord vli2048[129];

int vligetx(vliWord *x,unsigned char *s);

void vlirand(vliWord *x,unsigned int l);
void vlicopy(vliWord *x,vliWord *y);
void vligeti(vliWord *x,unsigned int l);
void vlihlf(vliWord *x);
void vliinc(vliWord *x);
void vliadd(vliWord *x,vliWord *y);
void vlidec(vliWord *x);
void vlisub(vliWord *x,vliWord *y);
void vlimul(vliWord *x,vliWord *y,vliWord *p);
void vlisqr(vliWord *x,vliWord *p);
void vlipow(vliWord *x,vliWord *m,vliWord *e);
void vligetb(vliWord *x,unsigned char *b,unsigned int l);
void vliputb(vliWord *x,unsigned char *b,unsigned int l);
void vligetd(vliWord *x,unsigned char *s);
void vliputd(vliWord *x,unsigned char *s,size_t ssize); //dtl added ssize
void vliputx(vliWord *x,char *s);
void vlipprime(vli x);
void vlirprime(vli x,vli y);

void vliprint(vliWord *x,char *label);

unsigned int vlibits(vli x);
unsigned int vlibytes(vli x);
unsigned int vlidiv(vliWord *x,vliWord *m,vliWord *q);

#endif
