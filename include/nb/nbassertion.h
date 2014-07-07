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
