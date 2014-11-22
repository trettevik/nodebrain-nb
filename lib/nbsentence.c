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
* 2014-10-20 eat 0.9.03 Switch from "_" to "@" for facet identifiers
* 2014-11-09 eat 0.9.03 Added support for NULL pointer for args
*            node@facet      - sentence->args=NULL
*                              API:  nbSET argSet=nbListOpen(arglist) 
*                              argSet and argList are both NULL
*            node@facet()    - args is empty list return NULL on first call
*                              API:  nbSET argSet=nbListOpen(arglist)
*                              argSet is null, but argList is not
*            node@facet(x)   - normal args list
*                              API:  nbSET argSet=nbListOpen(arglist)
*                              argSet points to cell x on first call
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
    if(*cell->facet->ident->value) outPut("@%s",cell->facet->ident->value);
    if(cell->args) printObject((NB_Object *)cell->args);
    }
  }

static void nbSentenceDestroy(NB_Sentence *cell){
  NB_Sentence *sentence,**sentenceP;
  NB_Hash *hash=cell->cell.object.type->hash;

  dropObject((NB_Object *)cell->term);
  if(cell->args) dropObject((NB_Object *)cell->args);
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

static NB_Object *evalSentence(NB_Sentence *sentence){
  NB_Node *node=(NB_Node *)sentence->term->def;
  if(node->cell.object.type!=nb_NodeType) return(nb_Unknown);
  if(sentence->facet==NULL) return(nb_Unknown);
  // 2014-11-21 eat - Support node redefinition
  // If the facet is not associated with the node's current skill, then see if we can fix it
  // Ideally this change would have been made when the node was redefined with a new skill.
  // However, a disabled subscription is difficult to find a node redefinition time.
  if(sentence->facet->skill!=node->skill){
    NB_Facet *facet;
    for(facet=node->skill->facet;facet!=NULL && facet->ident!=sentence->facet->ident;facet=(NB_Facet *)facet->object.next);
    if(facet) sentence->facet=facet; // correct facet 
    }
  return((*sentence->facet->eval)(node->context,node->skill->handle,node->knowledge,sentence->args));
  }

static void solveSentence(NB_Sentence *cell){
  if(cell->args) nbCellSolve_((NB_Cell *)cell->args);
  return;
  }

/**********************************************************************
* Private Cell Management Methods
**********************************************************************/

static void enableSentence(NB_Sentence *cell){
  nbAxonEnable((NB_Cell *)cell->term,(NB_Cell *)cell);
  if(cell->args) nbAxonEnable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }
static void disableSentence(NB_Sentence *cell){
  nbAxonDisable((NB_Cell *)cell->term,(NB_Cell *)cell);
  if(cell->args) nbAxonDisable((NB_Cell *)cell->args,(NB_Cell *)cell);
  }

/**********************************************************************
* Public Methods
**********************************************************************/

void nbSentenceInit(NB_Stem *stem){
  nb_SentenceType=nbObjectType(stem,"sentence",0,0,nbSentenceShow,nbSentenceDestroy);
  nbCellType(nb_SentenceType,solveSentence,evalSentence,enableSentence,disableSentence);
  }

struct NB_SENTENCE *nbSentenceNew(NB_Facet *facet,NB_Term *term,NB_List *args){
  NB_Sentence *sentence,**sentenceP;
  NB_Hash *hash=nb_SentenceType->hash;
  uint32_t hashcode;
  unsigned long h,*l,*r,zero=0;

  if(trace) outMsg(0,'T',"nbSentenceNew: called");
  l=(unsigned long *)&term;
  if(args) r=(unsigned long *)&args;
  else r=&zero;
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
  if(args) sentence->args=grabObject(args);
  else sentence->args=NULL;
  sentence->cell.object.hashcode=hashcode;
  sentence->cell.object.next=(NB_Object *)*sentenceP;
  *sentenceP=sentence;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&nb_SentenceType->hash);
  if(args) sentence->cell.level=args->cell.level+1;
  else sentence->cell.level=1;
  sentence->cell.object.value=nb_Disabled;
  return(sentence);
  }

