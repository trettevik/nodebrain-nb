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
* File:     nbmath.h 
* 
* Title:    Math Header (real numbers)
*
* Function:
*
*   This header defines routines that manage nodebrain math objects.
*
* See nbmath.c for more information.
*============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2002-08-31 Ed Trettevik (original prototype)
* 2003-03-15 eat 0.5.1  Modified for make file
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*============================================================================
*/
#ifdef NB_INTERNAL
#ifndef _NB_MATH_H_
#define _NB_MATH_H_

/* NOTE: we depend on MATH conforming to COND */
typedef struct MATH{         /* Math Function Object - one or two operands */
  struct NB_CELL   cell;     /* cell header */
  struct NB_OBJECT *left;    /* Left Operand     */
  struct NB_OBJECT *right;   /* Right Operand    */
  } NB_Math;

extern struct HASH *mathH;
extern struct MATH *mathFree;

extern struct TYPE *mathTypeInv;
extern struct TYPE *mathTypeAdd;
extern struct TYPE *mathTypeSub;
extern struct TYPE *mathTypeMul;
extern struct TYPE *mathTypeDiv;

void initMath(NB_Stem *stem);
NB_Math *useMath(struct TYPE *type,NB_Object *left,NB_Object *right);

void printMathAll(void);

#endif
#endif

#ifndef _NB_MATH_API_H_
#define _NB_MATH_API_H_

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbFunctionD_D(nbCELL context,char *name,double (*function)(double x));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbFunctionD_DD(nbCELL context,char *name,double (*function)(double x,double y));

#endif
