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
  uint32_t   mask;         // one less than size of hash table - modulo=mask+1
  uint32_t   objects;      // Number of objects in the hash
  uint32_t   limit;        // Number of objects that trigger a doubling of the hash table
  void *vect[1];           // Colision pointers - ptr to first object in list
  } NB_Hash;

void nbHashInit(NB_Stem *stem);
NB_Hash *nbHashNew(size_t modulo);  // 2012-12-31 eat - VID 4947
//void destroyHash(NB_Object object);
void nbHashShow(struct HASH *hash,char *label,NB_Type *type);
void nbHashStats(void);
void nbHashGrow(NB_Hash **hashP);

// This hashing algorithm is known as djb2 - credited to Daniel J. Bernstein
// The constant of 5381 is sometimes used instead of 261
#define NB_HASH_STR(HASHCODE,STR){ \
  int c; \
  const unsigned char *s=(const unsigned char *)STR; \
  HASHCODE=261; \
  while((c=*s++)) HASHCODE=((HASHCODE<<5)+HASHCODE)^c; \
  } 

#endif // NB_INTERNAL

#endif
