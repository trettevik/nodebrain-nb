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
* 2002/08/31 Ed Trettevik (original prototype)
*=============================================================================
*/
struct REAL{
  struct NB_OBJECT object;   /* object header */
  double value;              /* real number value */
  };

typedef struct REAL NB_Real;

extern struct HASH *realH;
extern struct TYPE *realType;

void *hashReal();
void initReal();
void printReal();
void printRealAll();
void destroyReal();
struct REAL *parseReal();

extern struct REAL *useReal();
struct REAL *newReal();

