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
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
*=============================================================================
*/
#ifndef _NB_VLI_H_
#define _NB_VLI_H_

#include "nbstd.h"

/*
*  Define the vli word size and data types with various numbers of bits.
*/

typedef unsigned short vliWord;  /* word size is 16 bits */

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

int vligetx();
void vliputx();

void vlirand();
void vlicopy();
void vligeti();
void vlihlf();
void vliinc();
void vliadd();
void vlidec();
void vlisub();
void vlimul();
void vlisqr();
void vlipow();
void vligetb();
void vliputb();
void vligetd(vliWord *x,unsigned char *s);
void vliputd();
void vliputx();
void vlipprime();
void vlirprime();

void vliprint();

unsigned int vlibits();
unsigned int vlibytes();
unsigned int vlidiv();

#endif
