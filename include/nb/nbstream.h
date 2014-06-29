/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     nbstream.h 
* 
* Title:    Event Stream Object Header
*
* Function:
*
*   This header defines routines that manage NodeBrain event stream objects.
*
* See nbstream.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2004-12-02 Ed Trettevik (original prototype)
* 2005-04-08 eat 0.6.2  API function definitions moved to nbapi.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_STREAM_H_
#define _NB_STREAM_H_

#if defined(NB_INTERNAL)

extern struct TYPE *nb_StreamType;

struct NB_STREAM{
  struct NB_OBJECT          object;    /* object header */
  struct STRING             *name;     /* name of stream */
  struct NB_STREAM_PRODUCER *producer; /* list of producers */
  struct NB_STREAM_SUBSCRIPTION *sub; /* list of subscriptions */
  };

typedef struct NB_STREAM NB_Stream;

struct NB_STREAM_PRODUCER{
  struct NB_OBJECT object;
  struct NB_STREAM *stream;
  void *handle;
  void (*handler)(NB_Cell *context,void *handle,char *topic,int state);
  };

typedef struct NB_STREAM_PRODUCER NB_StreamProducer;

struct NB_STREAM_SUBSCRIPTION{
  struct NB_STREAM_SUBSCRIPTION *next;
  struct NB_STREAM *stream;
  void   *session;
  void   (*subscriber)(NB_Cell *context,void *session,char *data);
  };

typedef struct NB_STREAM_SUBSCRIPTION NB_StreamSubscription;
  
void nbStreamInit(NB_Stem *stem);

#endif // NB_INTERNAL

// External API

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void *nbStreamProducerOpen(nbCELL context,char *streamName,void *handle,
  void (*handler)(nbCELL context,void *handle,char *topic,int state));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbStreamOpen(nbCELL context,char *streamName,void *session,void (*subscriber)(nbCELL context,void *session,char
*msg));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbStreamClose(nbCELL context,char *streamName,void *session,void (*subscriber)(nbCELL context,void *session,char
 *msg));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbStreamPublish(nbCELL producer,char *msg);

#endif
