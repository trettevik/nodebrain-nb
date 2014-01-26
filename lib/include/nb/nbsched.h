/*
* Copyright (C) 1998-2014 The Boeing Company
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
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
* 2014-01-12 eat 0.9.00 nbSchedInit replaces schedInit
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
typedef struct SCHED{
  struct NB_CELL cell;     /* schedule cell */
  struct STRING *symbol;   /* symbolic name */
  struct PERIOD period;    /* start and end times */
  time_t interval;         /* Fixed interval - w,d,h,m,s */
  time_t duration;         /* Fixed duration - w,d,h,m,s */
  struct tcQueue *queue;   /* Time queue */
  } NB_Sched;
  
void schedPrintDump(struct SCHED *sched);
void schedPrint(struct SCHED *sched);
void destroySched(struct SCHED *sched);
void nbSchedInit(NB_Stem *stem);
struct SCHED *newSched(NB_Cell *context,char symid,char *source,char **delim,char *msg,size_t msglen,int reuse);
time_t schedNext(time_t floor,struct SCHED *sched);

#endif
