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
* File:     nbcall.h
*
* Title:    Call Header (real numbers)
*
* Function:
*
*   This header defines routines that manage nodebrain "call" objects.
*
* See nbcall.c for more information.
*============================================================================
* Change History:
*
*    Date     Name/Change
* ----------  ---------------------------------------------------------------
* 2002-09-07  Ed Trettevik (original prototype)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning message. (gcc 4.5.0)
*============================================================================
*/
#ifndef _NB_CALL_H_
#define _NB_CALL_H_

#ifdef NB_INTERNAL
#include <nb/nbstem.h>

extern struct TYPE *callType;
extern struct TYPE *callTypeMod;
extern struct TYPE *callTypeTrace;

void NbCallInit(NB_Stem *stem);

NB_Object *NbCallUse(nbCELL context,char *ident,struct NB_LIST *list);
#endif // internal

extern int nbBindCellFunction(nbCELL context,char *name,void *function,char *bindingName);

#endif
