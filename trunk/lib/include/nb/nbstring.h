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
* File:     nbstring.h 
* 
* Title:    Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain string objects.
*
* See nbstring.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 Ed Trettevik (split out in version 0.4.1)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NBSTRING_H_
#define _NBSTRING_H_       /* never again */

#include <nb/nbstem.h>

struct STRING{
  struct NB_OBJECT object;    /* object header */
  char value[1];              /* length determined when created */
  };

typedef struct STRING NB_String;

extern struct HASH *strH;
extern NB_Type *strType;
extern struct STRING *stringFree;  /* this pointer should remain null */

void *hashStr(struct HASH *hash,char *cursor);
void initString(NB_Stem *stem);
void printStringRaw(struct STRING *string);
void printString(struct STRING *string);
void printStringAll(void);
void destroyString(struct STRING *str);
struct STRING *useString(char *value);

#endif
