/*
* Copyright (C) 1998-2012 The Boeing Company
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
*   void initAssertion();
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
void destroyAssertion(cond) struct COND *cond;{
  struct COND *lcond,**condP;

  if(trace) outMsg(0,'T',"destroyAssertion() called");
  condP=(struct COND **)hashCond(condH,cond->cell.object.type,cond->left,cond->right);
  if(*condP==cond) *condP=(struct COND *)cond->cell.object.next;
  else{
    for(lcond=*condP;lcond!=NULL && lcond!=cond;lcond=*condP)
      condP=(struct COND **)&lcond->cell.object.next;
    if(lcond==cond) *condP=(struct COND *)cond->cell.object.next;
    }
  dropObject(cond->left);
  nbCellDisable(cond->right,(NB_Cell *)cond);
  dropObject(cond->right);
  cond->cell.object.next=(NB_Object *)condFree;
  condFree=cond;
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
void assert(NB_Link *member,int mode){
  struct ASSERTION *assertion,*target;
  NB_Term   *term;
  NB_Node   *node;
  NB_Skill  *skill;
  NB_Facet  *facet;
  NB_List   *arglist;
  NB_Object *object;

  if(trace) outMsg(0,'T',"assert() called");
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
      }
    else if(assertion->target->type==condTypeNode){
      if(assertion->cell.object.type==assertTypeVal){
        if(object->value==nb_Disabled) object=object->type->compute(object);
        else object=(NB_Object *)grabObject(object->value); /* 2004/08/28 eat - grab added */
        }
      else if(assertion->cell.object.type!=assertTypeDef){
        outMsg(0,'L',"Cell definition assertion not support for node %s",term->word->value);
        return;
        }
      target=(struct ASSERTION *)assertion->target;
      term=(NB_Term *)target->target;
      node=(NB_Node *)term->def;
      if(node->cell.object.type!=nb_NodeType){
        outMsg(0,'E',"Term %s not defined as node",term->word->value);
        dropObject(object); /* 2004/08/28 eat */
        return;
        }
/* Need to test this and get it working - had problems with logfile listener
      if(clientIdentity!=node->owner){
        outMsg(0,'E',"Identity \"%s\" not owner of node \"%s\" and may not make assertions",clientIdentity->name->value,node->context->word->value);
        return;
        }
*/
      if((skill=node->skill)==NULL){
        outMsg(0,'E',"Node %s does not have an assertion method.",term->word->value);
        dropObject(object);
        return;
        }
      facet=skill->facet;
      arglist=(NB_List *)grabObject((NB_List *)target->object);
      if(mode&1) (*facet->alert)(term,skill->handle,node->knowledge,(NB_Cell *)arglist,(NB_Cell *)object);
      else (*facet->assert)(term,skill->handle,node->knowledge,(NB_Cell *)arglist,(NB_Cell *)object);
      dropObject(arglist);
      dropObject(object);   /* 2004/08/28 eat */
      nbCellPublish((NB_Cell *)term->def);
      nbCellPublish((NB_Cell *)term);
      }
    member=member->next;
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

void initAssertion(NB_Stem *stem){
  assertTypeDef=newType(stem,"==",condH,0,printAssertion,destroyAssertion);
  assertTypeVal=newType(stem,"=",condH,0,printAssertion,destroyAssertion);
  assertTypeRef=newType(stem,"=.=",condH,0,printAssertion,destroyAssertion);
  }

/*
*  Assertion constructor - we are using useCondition()
*/

//  API Functions

// Add an assertion to an assertion list
int nbAssertionAddTermValue(nbCELL context,nbSET *set,nbCELL term,nbCELL cell){
  NB_Link   *entry;
  NB_Object *object;                  
  object=(NB_Object *)useCondition(0,assertTypeVal,term,cell);
  if((entry=nb_LinkFree)==NULL) entry=(NB_Link *)nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=entry->next;                  
  entry->object=(NB_Object *)grabObject(object);
  entry->next=*set;
  *set=entry;
  return(0);
  }

// Assert
void nbAssert(nbCELL context,nbSET set){
  assert(set,0);
  }

// Alert
void nbAlert(nbCELL context,nbSET set){
  assert(set,1);
  }
