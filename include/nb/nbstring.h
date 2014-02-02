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

typedef struct STRING{
  struct NB_OBJECT object;    /* object header */
  char value[1];              /* length determined when created */
  } NB_String;

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
