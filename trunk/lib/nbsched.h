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
* 2003/03/15 eat 0.5.1  Split out from nbsched.c for make file.
*=============================================================================
*/

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
  
void schedPrintDump();
void schedPrint();
void destroySched();
void schedInit();
struct SCHED *newSched(NB_Cell *context,char symid,char *source,char **delim,char *msg,int reuse);
time_t schedNext();
