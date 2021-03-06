/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
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
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_SYNAPSE_H_
#define _NB_SYNAPSE_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

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

void nbSynapseInit(NB_Stem *stem);

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
