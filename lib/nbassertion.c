/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbassert.c 
*
* Title:    Assertion Functions 
*
* Function:
*
*   This file provides methods for nodebrain ASSERTION objects.  The ASSERTION
*   type extends COND.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbAssertionInit();
* 
* Description
*
*   A list of assertions is an optional component of a rule.
*
*     define <term> on(<condition>) [<assertions>] [:<command>]
*
*   Assertion objects do not register for cell change alerts.  They are
*   invoked in sequence when the rule fires, prior to execution of the
*   optional command.
*   
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-07 Ed Trettevik (original version introduced in 0.4.1)
* 2004-08-28 eat 0.6.1  object grab/drop modified for compute method change
* 2004-09-25 eat 0.6.1  node assertions limited to owner
* 2005-04-27 eat 0.6.2  included alert method support for cache module
* 2007-06-26 eat 0.6.8  Changed "expert" terminology to "node".
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-10-12 eat 0.8.12 Replaced malloc with nbAlloc
* 2014-01-12 eat 0.9.00 Removed hash pointer - referenced via type
* 2014-01-12 eat 0.9.00 nbAssertionInit replaces initAssertion
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
* 2014-06-15 eat 0.9.02 Added support for event transient terms
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *assertTypeDef=NULL;
struct TYPE *assertTypeVal=NULL;
struct TYPE *assertTypeRef=NULL;

/**********************************************************************
* Private Object Methods
**********************************************************************/
void printAssertion(struct ASSERTION *assertion){
  if(assertion==NULL) outPut("(?)");
  else{
    printObject(assertion->target);
    outPut("%s",assertion->cell.object.type->name);
    printObject(assertion->object);
    }
  }

/*
*  Assertion destructor
*    This is a copy
*/
void destroyAssertion(struct COND *cond){
  if(trace) outMsg(0,'T',"destroyAssertion() called");
  dropObject(cond->left);
  nbCellDisable(cond->right,(NB_Cell *)cond);
  dropObject(cond->right);
  freeCondition(cond);
  if(trace) outMsg(0,'T',"destroyAssertion() returning");
  }

/**********************************************************************
* Private Function Calculation Methods
**********************************************************************/

/* none */

/**********************************************************************
* Private Function Management Methods
**********************************************************************/

/* none */

/**********************************************************************
* Public Methods
**********************************************************************/
void printAssertions(NB_Link *link){
  while(link!=NULL){
    printObject(link->object);
    if((link=link->next)!=NULL) outPut(",");
    }
  }

/*
*  Apply rule assertions
*
*   mode=0  - assert
*   mode=1  - alert
*   mode=2  - default (only set if unknown)
*/
void nbAssert(nbCELL context,nbSET member,int mode){
  //struct ASSERTION *assertion,*target;
  struct ASSERTION *assertion;
  NB_Sentence *sentence;
  NB_Term     *term;
  NB_Node     *node,*contextNode;
  NB_Skill    *skill;
  NB_Facet    *facet;
  NB_List     *arglist;
  NB_Object   *object;
  NB_Link     *transientRoot=NULL,*transientLink,**transientLinkP,**transientNextP=&transientRoot;

  if(trace) outMsg(0,'T',"assert() called");
  contextNode=(NB_Node *)((NB_Term *)context)->def;
  while(member!=NULL){
    assertion=(struct ASSERTION *)member->object;
    object=assertion->object;
    term=(NB_Term *)assertion->target;
    if(assertion->target->type==termType){
      // 2011-03-19 eat - don't assert if default mode and value is not unknown
      if(mode&2 && term->def!=nb_Unknown);
      else if(assertion->cell.object.type==assertTypeDef)
        nbTermAssign(term,object);
      else if(assertion->cell.object.type==assertTypeRef){
        outMsg(0,'T',"assigning reference");
        ((NB_Node *)term->def)->reference=(NB_Term *)object;
        }
      else if(object->value==nb_Disabled){
        nbTermAssign(term,object->type->compute(object));
        dropObject(term->def); /* 2004/08/28 eat */
        }
      else nbTermAssign(term,object->value);
      if(mode==1 && term->cell.mode&NB_CELL_MODE_TRANSIENT){  // switch a cell flag to identify transient term
        // remove from old list
        for(transientLinkP=&contextNode->transientLink;*transientLinkP!=NULL && (*transientLinkP)->object!=(NB_Object *)term;transientLinkP=&(*transientLinkP)->next);
        if(*transientLinkP!=NULL){ // found it, so remove and reuse
          transientLink=*transientLinkP;
          *transientLinkP=transientLink->next;
          //transientLink->next=nb_LinkFree;
          //nb_LinkFree=transientLink;
          transientLink->next=NULL;
          }
        else{ // not found, so create new
          if((transientLink=nb_LinkFree)==NULL) transientLink=nbAlloc(sizeof(NB_Link));
          else nb_LinkFree=transientLink->next;
          transientLink->next=NULL;
          transientLink->object=(NB_Object *)term;  // we don't grab here, but an undefine of transient term must remove from this list
          }
        // insert in new list
        *transientNextP=transientLink;
        transientNextP=&(transientLink->next);
        }
      }
    else if(assertion->target->type==nb_SentenceType){
      if(assertion->cell.object.type==assertTypeVal){
        if(object->value==nb_Disabled) object=object->type->compute(object);
        else object=(NB_Object *)grabObject(object->value); /* 2004/08/28 eat - grab added */
        }
      else if(assertion->cell.object.type!=assertTypeDef){
        outMsg(0,'L',"Cell definition assertion not support for node %s",term->word->value);
        return;
        }
      sentence=(NB_Sentence *)assertion->target;
      term=sentence->term;
      node=(NB_Node *)term->def;
      if(node->cell.object.type!=nb_NodeType){
        outMsg(0,'E',"Term %s not defined as node",term->word->value);
        dropObject(object); /* 2004/08/28 eat */
        return;
        }
      if((skill=node->skill)==NULL){
        outMsg(0,'E',"Node %s does not have an assertion method.",term->word->value);
        dropObject(object);
        return;
        }
      facet=sentence->facet;
      arglist=(NB_List *)grabObject(sentence->args);
      if(mode&1)(*facet->alert)(term,skill->handle,node->knowledge,(NB_Cell *)arglist,(NB_Cell *)object);
      else (*facet->assert)(term,skill->handle,node->knowledge,(NB_Cell *)arglist,(NB_Cell *)object);
      dropObject(arglist);
      dropObject(object);   /* 2004/08/28 eat */
      nbCellPublish((NB_Cell *)term->def);
      nbCellPublish((NB_Cell *)term);
      }
    member=member->next;
    }
  if(mode==1){ // for alerts
    // Reset to Unknown the event transient terms from the last alert not set this time.
    for(transientLink=contextNode->transientLink;transientLink!=NULL;transientLink=contextNode->transientLink){
      term=(NB_Term *)transientLink->object;
      nbTermAssign(term,nb_Unknown); // reset event cell to Unknown
      contextNode->transientLink=transientLink->next;
      transientLink->next=nb_LinkFree;
      nb_LinkFree=transientLink;
      }
    contextNode->transientLink=transientRoot;  // Save list of event transient terms in this alert
    }
  }

void printAssertedValues(NB_Link *member){
  struct ASSERTION *assertion;
  outPut("(");
  while(member!=NULL){
    assertion=(struct ASSERTION *)member->object;
    printObject(assertion->target);
    outPut("=");
    printObject(assertion->object->value);
    if((member=member->next)!=NULL) outPut(",");
    }
  outPut(")");
  }

void nbAssertionInit(NB_Stem *stem){
  assertTypeDef=nbObjectType(stem,"==",0,TYPE_IS_ASSERT,printAssertion,destroyAssertion);
  assertTypeVal=nbObjectType(stem,"=",0,TYPE_IS_ASSERT,printAssertion,destroyAssertion);
  assertTypeRef=nbObjectType(stem,"=.=",0,TYPE_IS_ASSERT,printAssertion,destroyAssertion);
  }

/*
*  Assertion constructor - we are using useCondition()
*/

//  API Functions

// Add an assertion to an assertion list
int nbAssertionAddTermValue(nbCELL context,nbSET *set,nbCELL term,nbCELL cell){
  NB_Link   *entry;
  NB_Object *object;                  
  object=(NB_Object *)useCondition(assertTypeVal,term,cell);
  if((entry=nb_LinkFree)==NULL) entry=(NB_Link *)nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=entry->next;                  
  entry->object=(NB_Object *)grabObject(object);
  entry->next=*set;
  *set=entry;
  return(0);
  }
