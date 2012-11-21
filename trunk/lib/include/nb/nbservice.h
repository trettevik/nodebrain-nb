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
* File:     nbservice.h 
*
* Title:    Service Interface
*
* Function:
*
*   This header defines functions to run as a Unix daemon or Windows service.
*   
* See nbservice.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-16 eat 0.5.2  Created to help with modular compiling.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_SERVICE_H_
#define _NB_SERVICE_H_

#if defined(NB_INTERNAL)

#if defined(WIN32)
void nbwCommand(struct NB_CELL *context,char *cmdverb,char *cursor);
#endif

void nbwServiceStopped(void);
void daemonize(void);

#endif // NB_INTERNAL

// External API

extern int  agent;            /* running as daemon */

#if defined(WIN32)
_declspec (dllexport)
#else
extern
#endif
int nbService(int (*serviceMain)(int argc,char *argv[]),int argc,char *argv[]);

#endif

