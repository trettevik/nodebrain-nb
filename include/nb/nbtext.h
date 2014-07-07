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
* File:     nbtext.h 
* 
* Title:    Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain text objects.
*
* See nbtext.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2011-11-05 Ed Trettevik - original prototype
*=============================================================================
*/
#ifndef _NBTEXT_H_
#define _NBTEXT_H_       /* never again */

#include <nb/nbstem.h>

typedef struct NB_TEXT{
  struct NB_OBJECT object;    /* object header */
  char value[1];              /* length determined when created */
  } NB_Text;

extern NB_Type *textType;

void initText(NB_Stem *stem);
void printText(NB_Text *text);
void destroyText(NB_Text *text);
NB_Text *nbTextLoad(char *filename);
NB_Text *nbTextCreate(char *textStr);

#endif
