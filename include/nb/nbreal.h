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
* File:     nbreal.h 
* 
* Title:    Real Number Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain real number objects.
*
* See nbreal.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 Ed Trettevik (original prototype)
* 2014-01-12 eat 0.9.00 Removed has pointer - referencing via type
*=============================================================================
*/
#ifndef _NBREAL_H_
#define _NBREAL_H_       /* never again */

#include <nb/nbstem.h>

typedef struct REAL{
  struct NB_OBJECT object;   /* object header */
  double value;              /* real number value */
  } NB_Real;

extern struct TYPE *realType;

//void *hashReal(struct HASH *hash,double n);
void nbRealInit(NB_Stem *stem);
void printReal(struct REAL *real);
void printRealAll(void);
//void destroyReal(struct REAL *real);
struct REAL *parseReal(char *source);

extern struct REAL *useReal(double value);
//struct REAL *newReal(double value);

#endif
