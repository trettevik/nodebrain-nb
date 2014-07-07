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
* File:     nbaxon.h
*
* Title:    Axon Cell Header
*
* Function:
*
*   This header defines routines that manage nodebrain axon cells.
*
* See nbaxon.c for more information.
*============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2013-12-09 Ed Trettevik (original prototype introduced in 0.9.00)
*============================================================================
*/
#ifndef _NB_AXON_H_
#define _NB_AXON_H_

#include <nb/nbstem.h>

struct TYPE *nb_TypeAxonRelEq;

/* 
* Relational Equal Operator Axon Object 
*/
typedef struct NB_AXON_REL{  // axon object for relational equality
  NB_Cell     cell;          // object header
  NB_Cell     *pub;          // publishing cell
  union{
    NB_Cell   *trueCell;     // NULL, nb_Unknown, or the one true RelEq cell with matching constant
    NB_Cell   *falseCell;    // NULL, nb_Unknown, or the one false RelNe cell with matching constant
    NB_Object *value;        // nb_Unknown, or current object value  (LT or GT)
    NB_Real   *real;         // nb_Unknown, or current real value (LT or GT)
    NB_String *string;       // nb_Unknown, or current real value (LT or GT)
    };
  } NB_AxonRel;

// Functions

void nbAxonInit(NB_Stem *stem);
void nbAxonEnableRelEq(nbCELL pub,struct COND *cond);
void nbAxonDisableRelEq(nbCELL pub,struct COND *cond);
void nbAxonEnableRelNe(nbCELL pub,struct COND *cond);
void nbAxonDisableRelNe(nbCELL pub,struct COND *cond);
void nbAxonEnableRelRange(nbCELL pub,struct COND *cond);
void nbAxonDisableRelRange(nbCELL pub,struct COND *cond);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbAxonEnable(nbCELL pub,nbCELL sub);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbAxonDisable(nbCELL pub,nbCELL sub);

#endif
