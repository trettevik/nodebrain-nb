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
* File:     nbhash.h 
* 
* Title:    Hash Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain hash objects.
*
* See nbhash.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/08/31 Ed Trettevik (split out from nbobject.h in version 0.4.1)
*=============================================================================
*/
#ifndef _NB_HASH_H_
#define _NB_HASH_H_

#include <nbobject.h>
 
#if defined(NB_INTERNAL)

extern struct TYPE *typeHash;

struct HASH{               /* Hashing table */
  struct NB_OBJECT object; /* Special class to distinquish HASH from TERM */
  long modulo;             /* Size of hashing table */
  void *vect[1];           /* Colision pointers - ptr to first object in list */
  };

typedef struct HASH NB_Hash;

void initHash();
struct HASH *newHash();
void destroyHash();
void printHash();

#endif // NB_INTERNAL

#endif
