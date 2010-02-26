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
* File:     nbbfi.h 
* 
* Title:    Binary Function of Integer (BFI) Header
*
* Function:
*
*   This header defines routines that implement NodeBrain's "binary
*   function of integer" system.
*
* See nbbfi.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003/03/15 eat 0.5.1  Created to conform to new make file
*=============================================================================
*/
/**************************************************************************
* Index Data Structure
**************************************************************************/

struct bfiindex {
  struct bfiindex *next;
  int type;
  int from;
  int to;
  };

/* bfiindex.type values */
#define Composite 0
#define Simple    1
#define Range     2
#define Span      3

struct bfiindex *bfiIndexParse();
void   bfiIndexPrint();

/*************************************************************************
* Segment Data Structure
*************************************************************************/

struct bfiseg {            /* bfi segment, where f(i) is truth */
  struct bfiseg *prior;    /* prior segment */
  struct bfiseg *next;     /* next  segment  */
  long start;              /* start point - inclusive */
  long end;                /* end   point - exclusive */
  };

typedef struct bfiseg *bfi;
extern bfi bfifree;

bfi bfiAlloc();
bfi bfiNew();
bfi bfiDomain();
void bfiInsert();
void bfiInsertUnique();
bfi bfiRemove();
bfi bfiDispose();
bfi bfiCopy();
int bfiEval();
int bfiCompare();
void bfiPrint();
bfi bfiParse();
bfi bfiKnow();
bfi bfiUntil_();
bfi bfiYield_();
bfi bfiConflict_();
bfi bfiOr_();
bfi bfiOre_();
bfi bfiAnd_();
bfi bfiNot_();
bfi bfiXor_();
bfi bfiXore_();
bfi bfiIndexOne();
bfi bfiIndex();
bfi bfiReject();
bfi bfiSelect();
bfi bfiIndexedSelect();
bfi bfiUnion();
bfi bfiUntil();
bfi bfiYield();
bfi bfiAnd();
bfi bfiOre();
bfi bfiOr();
bfi bfiXor();
bfi bfiXore();

