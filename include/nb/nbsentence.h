/*
* Copyright (C) 2013-2014 Ed Trettevik <eat@nodebrain.org>
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
