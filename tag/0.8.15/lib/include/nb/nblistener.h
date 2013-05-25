/*
* Copyright (C) 1998-2013 The Boeing Company
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
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nblistener.h 
* 
* Title:    Listener Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain listener objects.
*
* See nblistener.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/09/30 Ed Trettevik - original prototype
*
* 2003/03/08 eat 0.5.1  Included "interpreter" parameter to listener.
*            This is needed to support functionality of depricated PIPE
*            command.  This is done now as part of a modification to implement
*            LOG listeners to get in sync with the documented.
* 2004/04/13 eat 0.6.0  Included simplied listener structure NB_LISTENER.
*            This is part of the API for skill modules and drivers.
* 2006/05/09 eat 0.6.6  Removed symbolic context for translators---they do it themselves now
* 2008/03/08 eat 0.7.0  Removed file, pos, pipe from listener
* 2008/03/09 eat 0.7.0  Moved old listener structure to nbprotocol.h
* 2010/01/02 eat 0.7.7  Included type to enable separate read and write listeners
*=============================================================================
*/
#ifndef _NB_LISTENER_H_
#define _NB_LISTENER_H_

#if defined(NB_INTERNAL)

extern int  serve;           /* set to zero to stop server */

typedef struct NB_LISTENER{
  struct NB_LISTENER *next;
  struct NB_CELL   *context;
  int               type;    // 0 - read, 1 - write
  int               fildes;  /* file descriptor */
#if defined(WIN32)
  WSAEVENT          hEvent;  // Windows socket event
#endif
  void             *session; /* session handle */
  void             (*handler)(struct NB_CELL *context,int fildes,void *session);
  } NB_Listener;

#endif // NB_INTERNAL

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerEnableOnDaemon(nbCELL context);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerAdd(nbCELL context,int fildes,void *session,void (*handler)(nbCELL context,int fildes,void *));

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerAddWrite(nbCELL context,int fildes,void *session,void (*handler)(nbCELL context,int fildes,void *));

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerReplace(nbCELL context,int fildes,void *session,void (*handler)(nbCELL context,int fildes,void *));

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerReplaceWrite(nbCELL context,int fildes,void *session,void (*handler)(nbCELL context,int fildes,void *));

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerRemove(nbCELL context,int fildes);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerRemoveWrite(nbCELL context,int fildes);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerStart(nbCELL context);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerStop(nbCELL context);

#endif
