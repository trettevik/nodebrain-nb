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
* 2002/08/31 Ed Trettevik (split out from nbobject.h in version 0.4.1)
* 2010-02-28 eat 0.4.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-12-31 eat 0.8.13 Checker updates
*=============================================================================
*/
#ifndef _NB_HASH_H_
#define _NB_HASH_H_

#include <nb/nbobject.h>
 
#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

extern struct TYPE *typeHash;

struct HASH{               /* Hashing table */
  struct NB_OBJECT object; /* Special class to distinquish HASH from TERM */
  long   modulo;             /* Size of hashing table */  
  void *vect[1];           /* Colision pointers - ptr to first object in list */
  };

typedef struct HASH NB_Hash;

void initHash(NB_Stem *stem);
struct HASH *newHash(size_t modulo);  // 2012-12-31 eat - VID 4947
void destroyHash(NB_Object object);
void printHash(struct HASH *hash,char *label,NB_Type *type);

#endif // NB_INTERNAL

#endif
