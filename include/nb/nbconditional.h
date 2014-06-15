/*
* Copyright (C) 2014      Ed Trettevik <eat@nodebrain.org>
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
* File:     nbconditional.h
*
* Title:    Header for Conditional Cell Routines
*
* See nbconditional.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2014-04-12 Ed Trettevik - original prototype introduced in 0.9.01
*=============================================================================
*/
#ifndef _NB_CONDITIONAL_H_
#define _NB_CONDITIONAL_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

extern struct TYPE *nb_ConditionalType;

/*
*  Sentence Cell
*
*    We previously used a COND structure for sentence cells.  But to accomodate
*    additional parameter (facet and receptor) we now create a unique structure.
*/
typedef struct NB_CONDITIONAL{// conditional cell structure
  struct NB_CELL cell;        // cell header
  struct NB_CELL *condition;  // cell whose value is checked
  struct NB_CELL *ifTrue;     // if condition value is true, return value of this cell
  struct NB_CELL *ifFalse;    // if condition value is false, return value of this cell
  struct NB_CELL *ifUnknown;  // if condition value is unknown, return value of this cell
  } NB_Conditional;

void nbConditionalInit(NB_Stem *stem);
struct NB_CONDITIONAL *nbConditionalUse(nbCELL condition,nbCELL true,nbCELL false,nbCELL unknown);

#endif // NB_INTERNAL

// External API

#endif
