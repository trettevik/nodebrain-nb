/*
* Copyright (C) 1998-2013 The Boeing Company
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
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbsentence.c
*
* Title:    Sentence Management Routines
*
* Function:
*
*   This file provides routines that manage Nodebrain sentences.  A sentence
*   is an expresion of the form <term>(<argList>) where term references a
*   node.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbSentenceInit();
*
*   NB_Node *nbSentenceParse();
*
* Description
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/07/05 Ed Trettevik (original prototype version 0.2.8)
*=============================================================================
*/
#include <nb/nbi.h>

NB_Type *nb_SentenceType=NULL;

/**********************************************************************
* Private Object Methods
**********************************************************************/

static void nbSentenceShow(struct NB_SENTENCE *cell){
  if(cell==NULL) outPut("(?)");
  else{
    printObject((NB_Object *)cell->term);
    if(*cell->facet->ident->value) outPut("_%s",cell->facet->ident->value);
    printObject((NB_Object *)cell->args);
    }
  }

static void nbSentenceDestroy(struct NB_SENTENCE *cell){
  dropObject((NB_Object *)cell->term);
  dropObject((NB_Object *)cell->args);
  nbFree(cell,sizeof(cell));
  }

/**********************************************************************
* Private Cell Calculation Methods
**********************************************************************/

static NB_Object *evalSentence(struct NB_SENTENCE *cell){
  NB_Node *node=(NB_Node *)cell->term->def;
  if(node->cell.object.type!=nb_NodeType) return(nb_Unknown);
  if(cell->facet==NULL) return(nb_Unknown);
  return((*cell->facet->eval)(node->context,node->skill->handle,node->knowledge,cell->args));
  }

static void solveSentence(struct NB_SENTENCE *cell){
  nbCellSolve_((NB_Cell *)cell->args);
  return;
  }

/**********************************************************************
* Private Cell Management Methods
**********************************************************************/

static void enableSentence(struct NB_SENTENCE *cell){
  nbCellEnable((NB_Cell *)cell->term,(NB_Cell *)cell);
  nbCellEnable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }
static void disableSentence(struct NB_SENTENCE *cell){
  nbCellDisable((NB_Cell *)cell->term,(NB_Cell *)cell);
  nbCellDisable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }

/**********************************************************************
* Public Methods
**********************************************************************/

void nbSentenceInit(NB_Stem *stem){
  nb_SentenceType=newType(stem,"nodeSentence",NULL,0,nbSentenceShow,nbSentenceDestroy);
  nbCellType(nb_SentenceType,solveSentence,evalSentence,enableSentence,disableSentence);
  }

struct NB_SENTENCE *nbSentenceNew(NB_Facet *facet,NB_Term *term,NB_List *args){
  struct NB_SENTENCE *sentence;
  // include logic here to make sure sentences are unique - no duplicates
  // if found, return it and don't create a new one
  sentence=(struct NB_SENTENCE *)nbCellNew(nb_SentenceType,NULL,sizeof(struct NB_SENTENCE));
  sentence->facet=facet;  // don't grab because it is a permenant object
  sentence->term=grabObject(term);
  sentence->args=grabObject(args);
  return(sentence);
  }

