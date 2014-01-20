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
* 2002-08-31 Ed Trettevik (split out from nbobject.h in version 0.4.1)
* 2010-02-28 eat 0.4.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-12-31 eat 0.8.13 Checker updates
* 2014-01-12 eat 0.9.00 Included nbHashGrow and nbHashStats functions
*=============================================================================
*/
#ifndef _NB_HASH_H_
#define _NB_HASH_H_

#include <nb/nbobject.h>
 
#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

extern struct TYPE *typeHash;

typedef struct HASH{       // Hashing table
  struct NB_OBJECT object; // Special type to distinquish HASH from TERM
  uint32_t   modulo;       // Size of hashing table
  uint32_t   objects;      // Number of objects in the hash
  void *vect[1];           // Colision pointers - ptr to first object in list
  } NB_Hash;

void nbHashInit(NB_Stem *stem);
struct HASH *newHash(size_t modulo);  // 2012-12-31 eat - VID 4947
void destroyHash(NB_Object object);
void printHash(struct HASH *hash,char *label,NB_Type *type);
void nbHashStats(void);
void nbHashGrow(NB_Hash **hashP);

// This algorithm is based on Perl.
// It is the "One-at-a-time" algorithm by Bob Jenkins (http://burtleburtle.net/bob/hash/doobs.html)
#define NB_HASH_STR(hash,str){ \
  register const char * const s_PeRlHaSh_tmp = str; \
  register const unsigned char *s_PeRlHaSh = (const unsigned char *)s_PeRlHaSh_tmp; \
  register int32_t i_PeRlHaSh = strlen(str); \
  register uint32_t hash_PeRlHaSh = hash; \
  while(i_PeRlHaSh--) { \
    hash_PeRlHaSh += *s_PeRlHaSh++; \
    hash_PeRlHaSh += (hash_PeRlHaSh << 10); \
    hash_PeRlHaSh ^= (hash_PeRlHaSh >> 6); \
    } \
  hash_PeRlHaSh += (hash_PeRlHaSh << 3); \
  hash_PeRlHaSh ^= (hash_PeRlHaSh >> 11); \
  (hash) = (hash_PeRlHaSh + (hash_PeRlHaSh << 15)); \
  }

#endif // NB_INTERNAL

#endif
