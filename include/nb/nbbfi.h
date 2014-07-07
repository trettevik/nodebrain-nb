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
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-12-31 eat 0.8.13 Checker updates
*=============================================================================
*/
/**************************************************************************
* Index Data Structure
**************************************************************************/

struct bfiindex{
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

struct bfiindex *bfiIndexParse(char *s,char *msg,size_t msglen); // 2012-12-31 eat - VID 5210,5457,5136, - msglen from int to size_t
void   bfiIndexPrint(struct bfiindex *index);

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

bfi bfiAlloc(void);
bfi bfiNew(long start,long end);
bfi bfiDomain(bfi g,bfi h);
void bfiInsert(bfi f,long start,long end);
void bfiInsertUnique(bfi f,long start,long end);
bfi bfiRemove(bfi s);
bfi bfiDispose(bfi f);
bfi bfiCopy(bfi g);
int bfiEval(bfi f,long i);
int bfiCompare(bfi g,bfi h);
void bfiPrint(bfi f,char *label);
bfi bfiParse(char *s);
bfi bfiKnow(bfi g);
bfi bfiUntil_(bfi g);
bfi bfiYield_(bfi g);
bfi bfiConflict_(bfi g);
bfi bfiOr_(bfi g);
bfi bfiOre_(bfi g);
bfi bfiAnd_(bfi g);
bfi bfiNot_(bfi g);
bfi bfiXor_(bfi g);
bfi bfiXore_(bfi g);
bfi bfiIndexOne(bfi g,int i);
bfi bfiIndex(bfi g,struct bfiindex *index);
bfi bfiReject(bfi g,bfi h);
bfi bfiSelect(bfi g,bfi h);
bfi bfiIndexedSelect(bfi g,bfi h,struct bfiindex *index);
bfi bfiUnion(bfi g,bfi h);
bfi bfiUntil(bfi g,bfi h);
bfi bfiYield(bfi g,bfi h);
bfi bfiAnd(bfi g,bfi h);
bfi bfiOre(bfi g,bfi h);
bfi bfiOr(bfi g,bfi h);
bfi bfiXor(bfi g,bfi h);
bfi bfiXore(bfi g,bfi h);

