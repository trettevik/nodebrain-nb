/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     nbcall.c 
*
* Title:    Call Functions 
*
* Function:
*
*   This file provides methods for nodebrain CALL objects.  The CALL type
*   extends COND.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initCall();
* 
* Description
*
*   The CALL object type provides an easy way to extend nodebrain
*   functionality.  Function can be included in this file to support any
*   number of operations on object lists.  The list members may be any
*   cell expression as illustrated by the following example.
*
*      _mod(3*a,b/2)
*
*   The initCall() method has the responsibility of creating an individual
*   object type for each such function.  The "eval" method for the type
*   must perform the desired operation on the list members to produce and
*   publish a new value each time a new value is published for a list
*   member.
*   
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-07 Ed Trettevik (original version introduced in 0.4.1)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning message. (gcc 4.5.0)
* 2013-01-13 eat 0.8.13 Checker updates - rosechecker
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *callTypeMod=NULL;

/**********************************************************************
* Private Object Methods
**********************************************************************/
void printCall(call) struct CALL *call;{
  if(call==NULL) outPut("(?)");
  else{
    outPut("%s",call->cell.object.type->name);
    printObject((NB_Object *)call->list);
    }
  }

/*
*  Call destructor  - using destroyCondition()
*/

/**********************************************************************
* Private Function Calculation Methods
**********************************************************************/

NB_Object *evalCallMod(struct CALL *call){
  NB_List *list=call->list;
  NB_Link *member;
  struct REAL *lreal,*rreal;

  if(list==NULL) return(nb_Unknown);  // 2013-01-13 eat - rosecheckers
  if((member=list->link)==NULL) return(nb_Unknown);
  if((lreal=(struct REAL *)member->object)==NULL) return(nb_Unknown);
  if((member=member->next)==NULL) return(nb_Unknown);
  if((rreal=(struct REAL *)member->object)==NULL) return(nb_Unknown);
  lreal=(struct REAL *)((NB_Object *)lreal)->value;
  rreal=(struct REAL *)((NB_Object *)rreal)->value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  if(lreal->value==0) return(nb_Unknown);
  return((NB_Object *)useReal(fmod(lreal->value,rreal->value)));
  }

void solveCall(struct CALL *call){
  nbCellSolve_((NB_Cell *)call->list);
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
void enableCall(struct CALL *call){
  nbAxonEnable((NB_Cell *)call->list,(NB_Cell *)call);
  }

void disableCall(struct CALL *call){
  nbAxonDisable((NB_Cell *)call->list,(NB_Cell *)call);
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void initCall(NB_Stem *stem){
  callTypeMod=nbObjectType(stem,"_mod",0,0,printCall,destroyCondition);
  nbCellType(callTypeMod,solveCall,evalCallMod,enableCall,disableCall);
  }

/*
*  Call constructor - we are using useCondition()
*/
