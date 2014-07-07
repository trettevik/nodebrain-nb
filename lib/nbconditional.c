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
* File:     nbconditional.c
*
* Title:    Conditional Cell Routines
*
* Function:
*
*   This file provides routines that manage NodeBrain conditional cells.  A
*   conditional cell checks the value of one cell and conditionally returns
*   its value or the value of another cell.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbConditionalInit();
*
*   NB_Node *nbConditionalParse();
*
* Description
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2014-04-12 Ed Trettevik - Introduced in 0.9.01
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

NB_Type *nb_ConditionalType=NULL;

/**********************************************************************
* Private Object Methods
**********************************************************************/

static void nbConditionalShow(NB_Conditional *conditional){
  outPut("(");
  printObject((NB_Object *)conditional->condition);
  if(conditional->ifTrue!=conditional->condition){ // have a substitute for true
    if(conditional->ifFalse==conditional->condition){
      if(conditional->ifUnknown==conditional->condition){
        outPut(" true ");
        printObject((NB_Object *)conditional->ifTrue);
        }
      else if(conditional->ifUnknown==conditional->ifTrue){
        outPut(" unfalse ");
        printObject((NB_Object *)conditional->ifTrue);
        }
      else{
        outPut(" true ");
        printObject((NB_Object *)conditional->ifTrue);
        outPut(" else unknown "); 
        printObject((NB_Object *)conditional->ifUnknown);
        }
      }
    else if(conditional->ifFalse==conditional->ifTrue){
      outPut(" known ");
      printObject((NB_Object *)conditional->ifTrue);
      if(conditional->ifUnknown!=conditional->condition){
        outPut(" else ");
        printObject((NB_Object *)conditional->ifUnknown);
        }
      }
    else if(conditional->ifUnknown==conditional->ifTrue){
      outPut(" false ");
      printObject((NB_Object *)conditional->ifFalse);
      outPut(" else ");
      printObject((NB_Object *)conditional->ifTrue);
      }
    else{
      outPut(" true ");
      printObject((NB_Object *)conditional->ifTrue);
      if(conditional->ifUnknown!=conditional->ifFalse){
        outPut(" else false ");
        printObject((NB_Object *)conditional->ifFalse);
        if(conditional->ifUnknown!=conditional->condition){
          outPut(" else ");
          printObject((NB_Object *)conditional->ifUnknown);
          }
        }
      else{
        outPut(" else ");
        printObject((NB_Object *)conditional->ifFalse);
        }
      }
    }
  else if(conditional->ifFalse!=conditional->condition){ // have substitute for false
    if(conditional->ifUnknown==conditional->ifFalse){
      outPut(" untrue ");
      printObject((NB_Object *)conditional->ifFalse);
      }
    else{
      outPut(" false ");
      printObject((NB_Object *)conditional->ifFalse);
      if(conditional->ifUnknown!=conditional->condition){
        outPut(" else unknown ");
        printObject((NB_Object *)conditional->ifUnknown);
        }
      }
    }
  else if(conditional->ifUnknown!=conditional->condition){ // have substitute for Unknown
    outPut(" unknown ");
    printObject((NB_Object *)conditional->ifUnknown);
    }
  // else no reason for a conditional
  outPut(")");
  }

static void nbConditionalDestroy(NB_Conditional *conditional){
  dropObject((NB_Object *)conditional->condition);
  dropObject((NB_Object *)conditional->ifTrue);
  dropObject((NB_Object *)conditional->ifFalse);
  dropObject((NB_Object *)conditional->ifUnknown);
  nbFree(conditional,sizeof(NB_Conditional));
  }

/**********************************************************************
* Private Cell Calculation Methods
**********************************************************************/

static NB_Object *evalConditional(NB_Conditional *conditional){
  if(conditional->condition->object.value==nb_Unknown) return(conditional->ifUnknown->object.value);
  if(conditional->condition->object.value==nb_False) return(conditional->ifFalse->object.value);
  return(conditional->ifTrue->object.value);
  }

static void solveConditional(NB_Conditional *conditional){
  nbCellSolve_(conditional->condition);
  if(conditional->condition->object.value==nb_Unknown) nbCellSolve_(conditional->ifUnknown);
  if(conditional->condition->object.value==nb_False) nbCellSolve_(conditional->ifUnknown);
  nbCellSolve_(conditional->ifTrue);
  }

/**********************************************************************
* Private Cell Management Methods
**********************************************************************/

static void enableConditional(NB_Conditional *conditional){
  nbCellEnable(conditional->condition,(nbCELL)conditional);
  nbCellEnable(conditional->ifTrue,(nbCELL)conditional);
  nbCellEnable(conditional->ifFalse,(nbCELL)conditional);
  nbCellEnable(conditional->ifUnknown,(nbCELL)conditional);
  }
static void disableConditional(NB_Conditional *conditional){
  nbCellDisable(conditional->condition,(nbCELL)conditional);
  nbCellDisable(conditional->ifTrue,(nbCELL)conditional);
  nbCellDisable(conditional->ifFalse,(nbCELL)conditional);
  nbCellDisable(conditional->ifUnknown,(nbCELL)conditional);
  }

/**********************************************************************
* Public Methods
**********************************************************************/

void nbConditionalInit(NB_Stem *stem){
  nb_ConditionalType=nbObjectType(stem,"nodeConditional",0,0,nbConditionalShow,nbConditionalDestroy);
  nbCellType(nb_ConditionalType,solveConditional,evalConditional,enableConditional,disableConditional);
  }

NB_Conditional *nbConditionalUse(nbCELL condition,nbCELL ifTrue,nbCELL ifFalse,nbCELL ifUnknown){
  NB_Conditional *conditional;
  // include logic here to make sure conditionals are unique - no duplicates
  // if found, return it and don't create a new one
  conditional=(NB_Conditional *)nbCellNew(nb_ConditionalType,NULL,sizeof(NB_Conditional));
  conditional->condition=grabObject(condition);  // don't grab because it is a permenant object
  conditional->ifTrue=grabObject(ifTrue);
  conditional->ifFalse=grabObject(ifFalse);
  conditional->ifUnknown=grabObject(ifUnknown);
  if(condition->object.value!=(NB_Object *)condition) conditional->cell.level=condition->level+1;
  else conditional->cell.level=1;
  if(ifTrue->object.value!=(NB_Object *)ifTrue && ifTrue->level>=conditional->cell.level)
    conditional->cell.level=ifTrue->level+1;
  if(ifFalse->object.value!=(NB_Object *)ifFalse && ifFalse->level>=conditional->cell.level)
    conditional->cell.level=ifFalse->level+1;
  if(ifUnknown->object.value!=(NB_Object *)ifUnknown && ifUnknown->level>=conditional->cell.level)
    conditional->cell.level=ifUnknown->level+1;
  return(conditional);
  }

