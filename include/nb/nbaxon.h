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
* File:     nbaxon.h
*
* Title:    Axon Cell Header
*
* Function:
*
*   This header defines routines that manage nodebrain axon cells.
*
* See nbaxon.c for more information.
*============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2013-12-09 Ed Trettevik (original prototype introduced in 0.9.00)
*============================================================================
*/
#ifndef _NB_AXON_H_
#define _NB_AXON_H_

#include <nb/nbstem.h>

extern struct TYPE *nb_TypeAxonRelEq;

/* 
* Relational Equal Operator Axon Object 
*/
typedef struct NB_AXON_REL{  // axon object for relational equality
  NB_Cell     cell;          // object header
  NB_Cell     *pub;          // publishing cell
  union{
    NB_Cell   *trueCell;     // NULL, nb_Unknown, or the one true RelEq cell with matching constant
    NB_Cell   *falseCell;    // NULL, nb_Unknown, or the one false RelNe cell with matching constant
    NB_Object *value;        // nb_Unknown, or current object value  (LT or GT)
    NB_Real   *real;         // nb_Unknown, or current real value (LT or GT)
    NB_String *string;       // nb_Unknown, or current real value (LT or GT)
    };
  } NB_AxonRel;

// Functions

void nbAxonInit(NB_Stem *stem);
void nbAxonEnableRelEq(nbCELL pub,struct COND *cond);
void nbAxonDisableRelEq(nbCELL pub,struct COND *cond);
void nbAxonEnableRelNe(nbCELL pub,struct COND *cond);
void nbAxonDisableRelNe(nbCELL pub,struct COND *cond);
void nbAxonEnableRelRange(nbCELL pub,struct COND *cond);
void nbAxonDisableRelRange(nbCELL pub,struct COND *cond);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbAxonEnable(nbCELL pub,nbCELL sub);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbAxonDisable(nbCELL pub,nbCELL sub);

#endif
