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
* File:     nbsched.h 
*
* Title:    Schedule Routines (prototype)
*
* Function:
*
*   This header defines routines that implement time conditions.
*
* See nbsched.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Split out from nbsched.c for make file.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-12-31 eat 0.8.13 schedInit n from int to size_t
*=============================================================================
*/
#ifndef _NB_SCHED_H_
#define _NB_SCHED_H_

#include <nb/nbstem.h>

extern struct TYPE *schedTypeTime;
extern struct TYPE *schedTypePulse;
extern struct TYPE *schedTypeDelay;

struct PERIOD{
  time_t start;         
  time_t end;
  };
  
extern struct PERIOD eternity;    /* 0 - maximum value */
extern struct HASH *schedH;      /* hash of schedule entries */

/*
*  Schedule Cell
*/    
struct SCHED{
  struct NB_CELL cell;     /* schedule cell */
  struct STRING *symbol;   /* symbolic name */
  struct PERIOD period;    /* start and end times */
  time_t interval;         /* Fixed interval - w,d,h,m,s */
  time_t duration;         /* Fixed duration - w,d,h,m,s */
  struct tcQueue *queue;   /* Time queue */
  };
  
void schedPrintDump(struct SCHED *sched);
void schedPrint(struct SCHED *sched);
void destroySched(struct SCHED *sched);
void schedInit(NB_Stem *stem,size_t n);  // 2012-12-31 eat - n from int to size_t
struct SCHED *newSched(NB_Cell *context,char symid,char *source,char **delim,char *msg,size_t msglen,int reuse);
time_t schedNext(time_t floor,struct SCHED *sched);

#endif
