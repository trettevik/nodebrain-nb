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
* File:     nbclock.h 
* 
* Title:    Internal Clock Header
*
* Function:
*
*   This header defines routines that manage internal timers.
*
* See nbclock.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/08/25 eat 0.5.1  Created to conform to new make file
* 2003/11/02 eat 0.5.5  Changed name to nbclock.h from nbevent.h
*            o Variable names were changed also
* 2006/12/08 eat 0.6.4  Conditionally shorten the MAXNAP time
*            On some systems (e.g. HPUX 11) a SIGCHLD will not interrupt a
*            select.  If we use a long MAXNAP, we could wait a long time before
*            issuing the wait that cleans up the zombie (defunct) child.  By
*            using a shorter MAXNAP we are able to shorten the time a child is
*            a zombie.  See nbmedulla.c for process handling.
*=============================================================================
*/
#ifndef _NB_CLOCK_H_
#define _NB_CLOCK_H_

#if defined(NB_INTERNAL)

#if defined(HPUX)      // use short nap time on systems where SIGCHLD doesn't interrupt select()
#define NB_MAXNAP 10       // maximum nap time - 10 seconds*/
#else
#define NB_MAXNAP 300      // maximum nap time - 5 minutes */
#endif

#define NB_CLOCK_GMT      0  /* time breakdown function - gmtime()          */
#define NB_CLOCK_LOCAL    1  /* time breakdown function - localtime()       */

extern time_t     nb_ClockTime;   /* current time */
extern time_t     nb_clockOffset; /* offset applied before breakdown */
extern int        nb_clockClock;  /* time breakdown function: see NB_CLOCK_GMT above */
extern int        nb_clockFormat; /* time display format */
                                  /* 0 "ssssssssss ", 1 "yyyy/mm/dd hh:mm:ss " */
extern int        nb_ClockAlerting;

struct NB_TIMER{
  struct NB_TIMER *next;    /* next timer */
  time_t          time;     /* expiration time */
  NB_Object       *object;  /* object to alert */
  };

typedef struct NB_TIMER NB_Timer;

void       nbClockInit();

struct tm *nbClockGetTm(int clock,time_t utc);
char      *nbClockToString(time_t utc,char *buffer);
void       nbClockShowTimers();
void       nbClockShowProcess(char *cursor);

#endif // NB_INTERNAL

//**********************************
// External API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbClockSetTimer(time_t time,nbCELL object);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbClockAlert();

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbClockToBuffer(char *buffer);

#endif
