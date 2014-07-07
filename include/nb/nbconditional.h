/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
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
