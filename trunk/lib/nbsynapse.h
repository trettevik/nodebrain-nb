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
* File:     nbsynapse.h 
* 
* Title:    Synapse Header
*
* Function:
*
*   This header defines routines that manage NB_Synapse objects.  An NB_Synapse
*   is an extension of NB_Object.  
*
* See nbsynapse.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2004/12/02 Ed Trettevik (original prototype introduced in 0.6.2)
* 2005/04/08 eat 0.6.2  API function definitions moved to nbapi.h
*=============================================================================
*/
#ifndef _NB_SYNAPSE_H_
#define _NB_SYNAPSE_H_

#if defined(NB_INTERNAL)

/* condition function values
*/

struct NB_SYNAPSE{             // synapse
  NB_Object       object;      // object header
  struct NB_CELL *context;     // context pointer
  void           *skillHandle; // skill handle (or handler specific alternative) 
  void           *nodeHandle;  // node handle (or handler specific alternative)
  struct NB_CELL *cell;        // cell to monitor
  void (*handler)(struct NB_CELL *context,void *skillHandle,void *nodeHandle,struct NB_CELL *cell);
  };

typedef struct NB_SYNAPSE NB_Synapse;

extern NB_Synapse  *nb_SynapsePool;

extern struct TYPE *nb_SynapseType;

void nbSynapseInit();

#endif // NB_INTERNAL

//******************************************************
// External API

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbSynapseOpen(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell,void (*handler)(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void *nbSynapseClose(nbCELL context,nbCELL synapse);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbSynapseSetTimer(nbCELL context,nbCELL synapse,int seconds);

#endif
