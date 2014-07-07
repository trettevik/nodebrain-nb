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
* File:     nbmacro.h 
* 
* Title:    Macro Object Header
*
* Function:
*
*   This header defines routines that manage nodebrain string macro objects.
*
* See nbmacro.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-11-17 Ed Trettevik (original prototype)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_MACRO_H_
#define _NB_MACRO_H_

struct NB_MACRO{
  struct NB_OBJECT  object;      /* object header */
  struct NB_TERM   *context;     /* local context */
  struct NB_LINK   *parms;       /* parameter list - positional terms */
  struct NB_LINK   *defaults;    /* default parameter assertion */
  struct STRING    *string;      /* model string to resolve */
  };

typedef struct NB_MACRO NB_Macro;

extern struct TYPE *nb_MacroType;

void nbMacroInit(NB_Stem *stem);
NB_Macro *nbMacroParse(NB_Cell *context,char **source);
NB_Link *nbMacroParseTermList(NB_Cell *context,char **source);
char *nbMacroSub(NB_Cell *context,char **cursorP);
NB_String *nbMacroString(NB_Cell *context,char **source);
int openCreate(char *filename, int flags, mode_t mode);
int openRead(char *filename, int flags);

#endif
