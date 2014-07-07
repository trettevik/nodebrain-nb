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
* File:     nbsentence.h
*
* Title:    Header for Sentence Cell Management Routines
*
* See nbsentence.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2013-12-20 Ed Trettevik - original prototype introduced in 0.9.00
*=============================================================================
*/
#ifndef _NB_SENTENCE_H_
#define _NB_SENTENCE_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

extern struct TYPE *nb_SentenceType;

/*
*  Sentence Cell
*
*    We previously used a COND structure for sentence cells.  But to accomodate
*    additional parameter (facet and receptor) we now create a unique structure.
*/
typedef struct NB_SENTENCE{// sentence cell structure
  struct NB_CELL cell;     // cell header
  struct NB_TERM  *term;   // term identifying the node
  struct NB_FACET *facet;  // facet providing the method
  struct NB_LIST  *args;   // parameter list of cells
  void *receptor;          // receptor - see axons - an evaluation accelerator concept
  } NB_Sentence;

void nbSentenceInit(NB_Stem *stem);
struct NB_SENTENCE *nbSentenceNew(NB_Facet *facet,NB_Term *term,NB_List *list);

#endif // NB_INTERNAL

// External API

#endif
