/*
* Copyright (C) 1998-2013 The Boeing Company
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
void destroyMath(struct MATH *math);
struct MATH *useMath(int inverse,struct TYPE *type,NB_Object *left,NB_Object *right);

void printMathAll(void);

#endif
