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
* File:     nbtime.h 
* 
* Title:    Time Condition Header
*
* Function:
*
*   This header defines routines that implement time conditions.
*
* See nbtime.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003/03/15 eat 0.5.1  Created to conform to new make file
*=============================================================================
*/
#ifndef _NBTIME_H_
#define _NBTIME_H_       /* never again */
extern long maxtime;
/*
*  Time Condition Definition
*
*  o We have two parameters for operations.  The use of these parameters
*    varies by operation.  We have named them left and right to relate
*    to expression syntax.
*  o By "Simple" we mean any of the operations that call tcSimple with
*    an alignment, step and duration.  These functions do not require
*    left and right parameters.
*  o Indexed selection requires two tcDef structures which we call "Select"
*    and "Index".  The "Select" operation is the same as we use for
*    non-indexed selection.
*
*         tcDef1:  Select{tcDef2, tcDef(right)}
*         tcDef2:  Index{tcDef(left), bfi index}
*
*/
struct tcDef{             /* time condition definition */
  bfi (*operation)();     /* operation to perform on left and right */
  void *left;             /* Simple - NULL */
                          /* Match  - duration stepper */
                          /* Prefix - NULL */
                          /* Infix  - tcDef for left side */
                          /* Index  - tcDef for left side */
                          /* Plan   - plan */
  void *right;            /* Simple - NULL */
                          /* Match  - tcParm */
                          /* Prefix - tcDef for right side */
                          /* Infix  - tcDef for right side */
                          /* Index  - bfiindex */
                          /* Plan   - thread */
  };

typedef struct tcDef *tc;  /* time condition pointer */

struct tcQueue{
  tc   tcdef;       /* Time Condition Definition */
  bfi  set;         /* Time Interval Set */
  };

typedef struct tcQueue *tcq;

struct NB_CALENDAR{        /* Calendar object */
  struct NB_OBJECT object;
  struct STRING *text;     /* source expression */
  tc tcdef;
  };

typedef struct NB_CALENDAR NB_Calendar;

extern NB_Term *nb_TimeCalendarContext;  /* calendar context */

/*
*  Time function parameter list structure
*
*  o  There are three forms of parameters as illustrated by the following
*     expression.
*
*        jan(1,1_5,1..5)
*
*        1    - day 1 to day 2 (single segment)
*        1_5  - day 1 to day 6 (single segment) 
*        1..5 - day 1,2,3,4,5  (multiple segments)
*
*  o  A negative value for stop[0] indicates when a multiple segment
*     range is specified.
*
*  o  This structure contains a stepper function and start and stop patterns.  The
*     first element [0] of pattern arrays specifies how many of the array elements are
*     used [1]-[6].  The stepper function is associated with the first unspecified
*     element (sec,min,hour,day,month,(year,decade,century,millenium)). The last
*     array element [7] is for special functions that don't map nicely to the broken
*     down time structure tm for alignment.  For these functions the parent parameters
*     are specified in [1]-[6] and function specific parameter is stored in [7].
*/    
struct tcParm{
  struct tcParm *next;
  long (*step)();     /* stepper */
  int start[8];       /* n,sec,min,hour,day,month,(year,decade,century,millenium),special */
  int stop[8];        /* n,sec,min,hour,day,month,(year,decade,century,millenium),special */
                      /* special value is for su(1.4) or week(7.50) */
  };

tc tcParse(NB_Cell *context,char **source,char *msg);
tcq tcQueueNew();
long tcQueueTrue();
long tcQueueFalse();

void tcPrintSeg();
long tcTime();

void nbTimeInit();
NB_Term *nbTimeDeclareCalendar(NB_Cell *context,char *ident,char **source,char *msg);
NB_Term *nbTimeLocateCalendar(char *ident);

bfi tcCast(long begin,long end,tc tcdef);
long tcStepYear(long timer,int n);
long tcStepQuarter(long timer,int n);
long tcStepMonth(long timer,int n);
long tcStepWeek(long timer,int n);
long tcStepDay(long timer,int n);
long tcStepHour(long timer,int n);
long tcStepMinute(long timer,int n);
long tcStepSecond(long timer,int n);
char *tcTimeString(char *str,long timer);

#endif
