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
* File:     nbassertion.h
*
* Title:    Assertion Header
*
* Function:
*
*   This header defines routines that manage nodebrain ASSERTION objects.
*   The ASSERTION object currently extends COND.
*
* See nbassert.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-14 Ed Trettevik (original prototype)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2014-01-12 eat 0.9.00 nbAssertionInit replaces initAssertion
* 2014-06-15 eat 0.9.02 Dropped nbAlert and Added mode to nbAssert to cover botha
*=============================================================================
*/
#ifndef _NB_ASSERTION_H_
#define _NB_ASSERTION_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

/* 
* NOTE: 
*
*   Because we are extending COND in a weird way, this structure must have
*   the same basic structure as COND (function and two pointers).
*   
*   There is no real requirement for an assertion to be a cell, since
*   it doesn't have to publish a value.  We are making it a cell for
*   now just to take advantage of COND methods.
*   
*/
struct ASSERTION{            /* Assertion cell */
  struct NB_CELL   cell;     /* cell header */
  struct NB_OBJECT *target;  /* term or node(args) to assign */
  /* struct NB_TERM   *term; */    /* term to assign */
  struct NB_OBJECT *object;  /* parameter list */
  };

typedef struct ASSERTION NB_Assertion;

extern struct TYPE *assertTypeDef;
extern struct TYPE *assertTypeVal;
extern struct TYPE *assertTypeRef;

void nbAssertionInit(NB_Stem *stem);
void printAssertions(NB_Link *link);
void printAssertedValues(NB_Link *member);

#endif // NB_INTERNAL

// External API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbAssertionAddTermValue(nbCELL context,nbSET *set,nbCELL term,nbCELL cell);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbAssert(nbCELL context,nbSET set,int mode);

#endif
