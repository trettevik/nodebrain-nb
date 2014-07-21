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
* 2013-12-20 Ed Trettevik - Introduced in 0.9.00
* 2014-01-25 eat 0.9.00 - Checker updates
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

NB_Type *nb_SentenceType=NULL;

/**********************************************************************
* Private Object Methods
**********************************************************************/

static void nbSentenceShow(NB_Sentence *cell){
  if(cell==NULL) outPut("(?)");
  else{
    printObject((NB_Object *)cell->term);
    if(*cell->facet->ident->value) outPut("_%s",cell->facet->ident->value);
    printObject((NB_Object *)cell->args);
    }
  }

static void nbSentenceDestroy(NB_Sentence *cell){
  NB_Sentence *sentence,**sentenceP;
  NB_Hash *hash=cell->cell.object.type->hash;

  dropObject((NB_Object *)cell->term);
  dropObject((NB_Object *)cell->args);
  sentenceP=(NB_Sentence **)&(hash->vect[cell->cell.object.hashcode&hash->mask]);
  for(sentence=*sentenceP;sentence!=NULL && sentence!=cell;sentence=*sentenceP)
    sentenceP=(NB_Sentence **)&sentence->cell.object.next;
  if(sentence==cell) *sentenceP=(NB_Sentence *)sentence->cell.object.next;
  hash->objects--;
  nbFree(cell,sizeof(NB_Sentence));
  }

/**********************************************************************
* Private Cell Calculation Methods
**********************************************************************/

static NB_Object *evalSentence(NB_Sentence *cell){
  NB_Node *node=(NB_Node *)cell->term->def;
  if(node->cell.object.type!=nb_NodeType) return(nb_Unknown);
  if(cell->facet==NULL) return(nb_Unknown);
  return((*cell->facet->eval)(node->context,node->skill->handle,node->knowledge,cell->args));
  }

static void solveSentence(NB_Sentence *cell){
  nbCellSolve_((NB_Cell *)cell->args);
  return;
  }

/**********************************************************************
* Private Cell Management Methods
**********************************************************************/

static void enableSentence(NB_Sentence *cell){
  nbAxonEnable((NB_Cell *)cell->term,(NB_Cell *)cell);
  nbAxonEnable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }
static void disableSentence(NB_Sentence *cell){
  nbAxonDisable((NB_Cell *)cell->term,(NB_Cell *)cell);
  nbAxonDisable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }

/**********************************************************************
* Public Methods
**********************************************************************/

void nbSentenceInit(NB_Stem *stem){
  nb_SentenceType=nbObjectType(stem,"nodeSentence",0,0,nbSentenceShow,nbSentenceDestroy);
  nbCellType(nb_SentenceType,solveSentence,evalSentence,enableSentence,disableSentence);
  }

struct NB_SENTENCE *nbSentenceNew(NB_Facet *facet,NB_Term *term,NB_List *args){
  NB_Sentence *sentence,**sentenceP;
  NB_Hash *hash=nb_SentenceType->hash;
  uint32_t hashcode;
  unsigned long h,*l,*r;

  if(trace) outMsg(0,'T',"nbSentenceNew: called");
  l=(unsigned long *)&term;
  r=(unsigned long *)&args;
  h=((*l>>3)+(*r>>3));
  h+=(h>>3);
  hashcode=h;
  sentenceP=(NB_Sentence **)&(hash->vect[hashcode&hash->mask]);
  for(sentence=*sentenceP;sentence!=NULL;sentence=*sentenceP){
    if(sentence->term==term && sentence->facet==facet && sentence->args==args) return(sentence);
    sentenceP=(NB_Sentence **)&sentence->cell.object.next;
    }
  sentence=(NB_Sentence *)nbCellNew(nb_SentenceType,NULL,sizeof(NB_Sentence));
  sentence->facet=facet;  // don't grab because it is a permenant object
  sentence->term=grabObject(term);
  sentence->args=grabObject(args);
  sentence->cell.object.hashcode=hashcode;
  sentence->cell.object.next=(NB_Object *)*sentenceP;
  *sentenceP=sentence;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&nb_SentenceType->hash);
  sentence->cell.level=args->cell.level+1;
  sentence->cell.object.value=nb_Disabled;
  return(sentence);
  }

