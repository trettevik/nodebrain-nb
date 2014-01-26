/*
* Copyright (C) 2013-2014 The Boeing Company
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
* File:     nbaxon.c 
*
* Title:    Axon Cell Functions 
*
* Function:
*
*   This file provides methods for nodebrain axon cells that provide
*   accelerated cell evaluation for some class of cells by use of a
*   reduction technique.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbAxonInit();
* 
* Description
*
*   A axon cell subscribes to a cell on behalf of multiple dependent
*   cells.  The axon cell manages the evaluation of the dependent cells,
*   all of which qualify for a technique that can reduce the number of
*   evaluations.
*
*   The subscription tree for a axon cell is ordered by value instead of
*   address.  The ordering routine is provided by the axon.  It is because
*   of the ordering that the axon cell is able to "know" that a subset
*   of the subscribers do not need to be evaluated because their value
*   would not change.
*
*   Axon objects implemented in this file may be closely related to operations
*   implemented in nbcondition.c.  We have split the axon routines out
*   to avoid cluttering nbcondition.c
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2013-12-09 Ed Trettevik (original version introduced in 0.9.00)
* 2014-01-25 eat 0.9.00 - CID 1164444 
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *nb_TypeAxonRelEq=NULL;
struct TYPE *nb_TypeAxonRelNe=NULL;
struct TYPE *nb_TypeAxonRelLtString=NULL;
struct TYPE *nb_TypeAxonRelLeString=NULL;
struct TYPE *nb_TypeAxonRelLtReal=NULL;
struct TYPE *nb_TypeAxonRelLeReal=NULL;
struct TYPE *nb_TypeAxonRelGtString=NULL;
struct TYPE *nb_TypeAxonRelGeString=NULL;
struct TYPE *nb_TypeAxonRelGtReal=NULL;
struct TYPE *nb_TypeAxonRelGeReal=NULL;

static NB_AxonRel *useAxonRel(NB_Type *type,NB_Cell *pub){
  NB_AxonRel *axon,**axonP;
  NB_Hash *hash=type->hash;
  uint32_t hashcode;

  hashcode=pub->object.hashcode;  // use the publishers hashcode
  axonP=(NB_AxonRel **)&(hash->vect[hashcode&hash->mask]);
  for(axon=*axonP;axon!=NULL;axon=*axonP){
    if(axon->pub==pub && axon->cell.object.type==type) return(axon);
    axonP=(NB_AxonRel **)&axon->cell.object.next;
    }
  axon=(NB_AxonRel *)nbCellNew(type,NULL,sizeof(NB_AxonRel));
  axon->cell.object.hashcode=hashcode;
  axon->cell.object.next=(NB_Object *)*axonP;
  *axonP=axon; 
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&type->hash);
  axon->pub=pub;
  axon->trueCell=NULL;
  return(axon);
  }

/**********************************************************************
* Private Object Methods
**********************************************************************/
static void printAxon(NB_AxonRel *axon){
  if(axon==NULL) outPut("(?)");
  else{
    outPut("%s(",axon->cell.object.type->name);
    printObject((NB_Object *)axon->pub);
    outPut(")");
    }
  }

/*
*  Call destructor  - using destroyCondition()
*/

/**********************************************************************
* Private Function Calculation Methods
**********************************************************************/

/*
*  Eval Rel Eq
*
*    Evaluate 1 or 2 of the potentially many subscribers
*/
static NB_Object *evalAxonRelEq(NB_AxonRel *axon){
  NB_TreeNode *treeNode=(NB_TreeNode *)axon->cell.sub;
  NB_Cell *trueCell;  // condition that is true
  void *right=axon->pub->object.value;

  // Search for subscriber with right operand equal to new value of pub
  if(right==nb_Unknown){
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    axon->trueCell=(NB_Cell *)nb_Unknown;
    }
  else if(axon->trueCell==(NB_Cell *)nb_Unknown){
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode) axon->trueCell=(NB_Cell *)treeNode->key;
    else axon->trueCell=NULL;
    }
  else{
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode){
      trueCell=(NB_Cell *)treeNode->key;
      if(trueCell!=axon->trueCell){ // This should always be true
        trueCell->object.value=NB_OBJECT_TRUE;
        nbCellPublish(trueCell);
        if(axon->trueCell){
          axon->trueCell->object.value=nb_False;
          nbCellPublish(axon->trueCell);
          }
        axon->trueCell=trueCell;
        }
      }
    else if(axon->trueCell){  // we have no match
      axon->trueCell->object.value=nb_False;
      nbCellPublish(axon->trueCell);
      axon->trueCell=NULL;
      }
    }
  return(nb_Unknown);
  }

/*
*  Eval Rel NE 
*
*    Evaluate 1 or 2 of the potentially many subscribers
*/
static NB_Object *evalAxonRelNe(NB_AxonRel *axon){
  NB_TreeNode *treeNode=(NB_TreeNode *)axon->cell.sub;
  NB_Cell *falseCell;  // condition that is false
  void *right=axon->pub->object.value;

  // Search for subscriber with right operand equal to new value of pub
  if(right==nb_Unknown){
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    axon->falseCell=(NB_Cell *)nb_Unknown;
    }
  else if(axon->falseCell==(NB_Cell *)nb_Unknown){
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode) axon->falseCell=(NB_Cell *)treeNode->key;
    else axon->falseCell=NULL;
    }
  else{
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode){
      falseCell=(NB_Cell *)treeNode->key;
      if(falseCell!=axon->falseCell){ // This should always be true
        falseCell->object.value=nb_False;
        nbCellPublish(falseCell);
        if(axon->falseCell){
          axon->falseCell->object.value=nb_True;
          nbCellPublish(axon->falseCell);
          }
        axon->falseCell=falseCell;
        }
      }
    else if(axon->falseCell){  // we have no match
      axon->falseCell->object.value=nb_True;
      nbCellPublish(axon->falseCell);
      axon->falseCell=NULL;
      }
    }
  return(nb_Unknown);
  }

/*
*  Evaluate axon managed relational condition for real numbers
*
*  Prototyping using single function that check condition types
*  Will revise so we are not checking types later
*/
static NB_Object *evalAxonRelReal(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode,*stack[32];
  NB_Object *value;
  NB_Object *condValue;
  double min,max;
  int s;
  int edge=0;

  value=axon->pub->object.value;   
  if(value->type==realType){                // if published value is a real number
    if(axon->real==NULL){                   // if prior value was not real number
      nbCellPublish((NB_Cell *)axon);
      axon->real=(NB_Real *)value;
      grabObject(axon->real);
      return(nb_Unknown);
      }
    if(axon->real->value<((NB_Real *)value)->value){  // if prior value was real number less than new
      min=axon->real->value;
      max=((NB_Real *)value)->value;
      if(axon->cell.object.type==nb_TypeAxonRelGtReal) condValue=nb_True,edge=-1;
      else if(axon->cell.object.type==nb_TypeAxonRelGeReal) condValue=nb_True,edge=1;
      else if(axon->cell.object.type==nb_TypeAxonRelLeReal) condValue=nb_False,edge=-1;
      else if(axon->cell.object.type==nb_TypeAxonRelLtReal) condValue=nb_False,edge=1;
      else{
        outMsg(0,'L',"evalAxonRelReal: unrecognized cell type");
        return(nb_Unknown);
        }
      }
    else{                                             // if prior value was real number greater than new
      max=axon->real->value;
      min=((NB_Real *)value)->value;
      if(axon->cell.object.type==nb_TypeAxonRelLtReal) condValue=nb_True,edge=1;
      else if(axon->cell.object.type==nb_TypeAxonRelLeReal) condValue=nb_True,edge=-1;
      else if(axon->cell.object.type==nb_TypeAxonRelGeReal) condValue=nb_False,edge=1;
      else if(axon->cell.object.type==nb_TypeAxonRelGtReal) condValue=nb_False,edge=-1;
      else{
        outMsg(0,'L',"evalAxonRelReal: unrecognized cell type");
        return(nb_Unknown);
        }
      }
    // build path to smallest number within range
    treeNode=axon->cell.sub;
    s=0;
    while(treeNode){
      if(((NB_Real *)((NB_Cond *)treeNode->key)->right)->value>min){
        if(((NB_Real *)((NB_Cond *)treeNode->key)->right)->value<max ||
           (((NB_Real *)((NB_Cond *)treeNode->key)->right)->value==max && edge==1)){
          stack[s]=treeNode;
          s++;
          }
        treeNode=treeNode->left;
        }
      else if(((NB_Real *)((NB_Cond *)treeNode->key)->right)->value==min && edge==-1){
        stack[s]=treeNode;
        s++;
        treeNode=treeNode->right;
        }
      else treeNode=treeNode->right;
      }
    // adjust each condition within range
    while(s>0){
      s--;
      treeNode=stack[s];
      ((NB_Object *)treeNode->key)->value=condValue; 
      nbCellPublish((NB_Cell *)treeNode->key);
      treeNode=treeNode->right;
      while(treeNode){
        if(((NB_Real *)((NB_Cond *)treeNode->key)->right)->value<max ||
           (((NB_Real *)((NB_Cond *)treeNode->key)->right)->value==max && edge==1)){
          stack[s]=treeNode;
          s++;
          }
        treeNode=treeNode->left;
        }
      }
    dropObject(axon->real);
    axon->real=(NB_Real *)value;
    grabObject(axon->real);
    }
  else if(axon->real){
    //outMsg(0,'T',"evalAxonRelReal: setting all subscribing conditions to Unknown");
    // set everything relational cell to unknown
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->real);
    axon->real=NULL;
    }
  return(nb_Unknown);
  }

static void nbAxonRelStringSweep(NB_AxonRel *axon,NB_Object *upValue,NB_Object *downValue,int edge){
  NB_TreeNode *treeNode,*stack[32];
  NB_Object *value,*newValue;
  int s;
  char *min,*max;
 
  value=axon->pub->object.value;
  if(strcmp(axon->string->value,((NB_String *)value)->value)<1){  // if prior value was string less than new
    min=axon->string->value;
    max=((NB_String *)value)->value;
    newValue=upValue;
    }
  else{                                             // if prior value was string greater than new
    max=axon->string->value;
    min=((NB_String *)value)->value;
    newValue=downValue;
    }
  treeNode=axon->cell.sub;
  s=0;
  while(treeNode){
    if(strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,min)>0){
      if(strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,max)<0 ||
         (strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,max)==0 && edge==1)){
        stack[s]=treeNode;
        s++;
        }
      treeNode=treeNode->left;
      }
    else if(strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,min)==0 && edge==-1){
      stack[s]=treeNode;
      s++;
      treeNode=treeNode->right;
      }
    else treeNode=treeNode->right;
    }
  // adjust each condition within range
  while(s>0){
    s--;
    treeNode=stack[s];
    ((NB_Object *)treeNode->key)->value=newValue;
    nbCellPublish((NB_Cell *)treeNode->key);
    treeNode=treeNode->right;
    while(treeNode){
      if(strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,max)<0 ||
         (strcmp(((NB_String *)((NB_Cond *)treeNode->key)->right)->value,max)==0 && edge==1)){
        stack[s]=treeNode;
        s++;
        }
      treeNode=treeNode->left;
      }
    }
  dropObject(axon->string);
  axon->string=(NB_String *)value;
  grabObject(axon->string);
  }

/*
*  Evaluate axon managed relational LT condition for string values
*/
static NB_Object *evalAxonRelLtString(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Object *value;

  value=axon->pub->object.value;
  if(value->type==strType){                // if published value is a string
    if(axon->string==NULL){                  // if prior value was not string
      nbCellPublish((NB_Cell *)axon);
      axon->string=(NB_String *)value;
      grabObject(axon->string);
      return(nb_Unknown);
      }
    nbAxonRelStringSweep(axon,nb_False,nb_True,-1);
    }
  else if(axon->string){
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->string);
    axon->string=NULL;
    }
  return(nb_Unknown);
  }

/*
*  Evaluate axon managed relational LT condition for string values
*/
static NB_Object *evalAxonRelLeString(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Object *value;

  value=axon->pub->object.value;
  if(value->type==strType){                // if published value is a string
    if(axon->string==NULL){                  // if prior value was not string
      nbCellPublish((NB_Cell *)axon);
      axon->string=(NB_String *)value;
      grabObject(axon->string);
      return(nb_Unknown);
      }
    nbAxonRelStringSweep(axon,nb_False,nb_True,1);
    }
  else if(axon->string){
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->string);
    axon->string=NULL;
    }
  return(nb_Unknown);
  }

/*
*  Evaluate axon managed relational LT condition for string values
*/
static NB_Object *evalAxonRelGtString(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Object *value;

  value=axon->pub->object.value;
  if(value->type==strType){                // if published value is a string
    if(axon->string==NULL){                  // if prior value was not string
      nbCellPublish((NB_Cell *)axon);
      axon->string=(NB_String *)value;
      grabObject(axon->string);
      return(nb_Unknown);
      }
    nbAxonRelStringSweep(axon,nb_True,nb_False,-1);
    }
  else if(axon->string){
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->string);
    axon->string=NULL;
    }
  return(nb_Unknown);
  }

/*
*  Evaluate axon managed relational LT condition for string values
*/
static NB_Object *evalAxonRelGeString(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Object *value;

  value=axon->pub->object.value;
  if(value->type==strType){                // if published value is a string
    if(axon->string==NULL){                  // if prior value was not string
      nbCellPublish((NB_Cell *)axon);
      axon->string=(NB_String *)value;
      grabObject(axon->string);
      return(nb_Unknown);
      }
    nbAxonRelStringSweep(axon,nb_True,nb_False,1);
    }
  else if(axon->string){
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->string);
    axon->string=NULL;
    }
  return(nb_Unknown);
  }

/*
*  Solve Axon - dummy function to let us know if it is attempted
*/
static void solveAxon(struct CALL *call){
  outMsg(0,'L',"Logic error - solveAxonRelEq called - axon cells should not be referenced like this");
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
static void enableAxonRel(NB_AxonRel *axon){   
  //outMsg(0,'T',"enableAxonRelEq: called");
  }

static void disableAxonRel(NB_AxonRel *axon){
  //outMsg(0,'T',"disableAxonRelEq: called");
  }

static void destroyAxonRel(NB_AxonRel *axon){
  NB_AxonRel *laxon,**axonP;
  NB_Hash *hash=axon->cell.object.type->hash;
  uint32_t hashcode;

  nbAxonDisable((NB_Cell *)axon->pub,(NB_Cell *)axon); // should we check first
  dropObject(axon->pub);
  hashcode=axon->cell.object.hashcode;
  axonP=(NB_AxonRel **)&(hash->vect[hashcode&hash->mask]);
  for(laxon=*axonP;laxon!=NULL && laxon!=axon;laxon=*axonP)
    axonP=(NB_AxonRel **)&laxon->cell.object.next;
  if(laxon) *axonP=(NB_AxonRel *)axon->cell.object.next;
  nbFree(axon,sizeof(NB_AxonRel)); 
  hash->objects--;
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbAxonInit(NB_Stem *stem){
  nb_TypeAxonRelEq=newType(stem,"AxonRelEq",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelEq,solveAxon,evalAxonRelEq,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelNe=newType(stem,"AxonRelNe",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelNe,solveAxon,evalAxonRelNe,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLtString=newType(stem,"AxonRelLtString",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelLtString,solveAxon,evalAxonRelLtString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLeString=newType(stem,"AxonRelLeString",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelLeString,solveAxon,evalAxonRelLeString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLtReal=newType(stem,"AxonRelLtReal",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelLtReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLeReal=newType(stem,"AxonRelLeReal",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelLeReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGtString=newType(stem,"AxonRelGtString",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelGtString,solveAxon,evalAxonRelGtString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGeString=newType(stem,"AxonRelGeString",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelGeString,solveAxon,evalAxonRelGeString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGtReal=newType(stem,"AxonRelGtReal",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelGtReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGeReal=newType(stem,"AxonRelGeReal",NULL,0,printAxon,destroyAxonRel);
  nbCellType(nb_TypeAxonRelGeReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  }


/*
*  Enable value change publication from a cell to a subscriber object.
*
*    o  It is safe to enable a NULL publisher or a non-publishing object.
*       These calls are ignored.  This is done so the caller doesn't need to
*       test object pointers before calling.
*
*    o  A subscriber is not required to be a cell---may be a simple object.
*
*    o  A tree structure is used to manage the set of all subscribers to a given cell.
*/
void nbAxonEnable(NB_Cell *pub,NB_Cell *sub){
  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  if(trace){
    outMsg(0,'T',"nbAxonEnable: called - linking subscriber");
    outPut("subscriber: ");
    printObject((NB_Object *)sub);
    outPut("\n");
    outPut("publisher : ");
    printObject((NB_Object *)pub);
    outPut(" = ");
    printObject(pub->object.value);
    outPut("\n");
    }

  if(sub!=NULL){
    NB_TreePath treePath;
    NB_TreeNode *treeNode;
    treeNode=(NB_TreeNode *)nbTreeLocate(&treePath,sub,(NB_TreeNode **)&pub->sub);
    if(treeNode==NULL){  // if not already a subscriber, then subscribe
      treeNode=(NB_TreeNode *)nbAlloc(sizeof(NB_TreeNode));
      treeNode->key=sub;
      nbTreeInsert(&treePath,treeNode);
      }
    }
  if(pub->object.value!=nb_Disabled) return; // already know the value
  pub->object.type->enable(pub); /* pub's enable method */
  pub->object.value=(NB_Object *)grabObject(pub->object.type->eval(pub));  /* pub's evaluation method */
  if(sub!=NULL && sub->object.value!=(NB_Object *)sub && sub->level<=pub->level){
    ((NB_Cell *)sub)->level=pub->level+1;
    nbCellLevel((NB_Cell *)sub);
    }
  if(trace){
    outMsg(0,'T',"nbAxonEnable() returning");
    outPut("Function: ");
    printObject((NB_Object *)pub);
    outPut("\nResult:");
    printObject(pub->object.value);
    outPut("\n");
    }
  }

/*
*  Cancel an object's subscription to cell changes.
*
*    A tree structure is used to manage all subscribers to a given cell.
*/
void nbAxonDisable(NB_Cell *pub,NB_Cell *sub){
  if(trace){
    outMsg(0,'T',"nbAxonDisable() called");
    printObject((NB_Object *)pub);
    outPut("\n");
    }
  if(pub->object.value==(NB_Object *)pub) return; /* static object */
  if(pub->object.value==nb_Disabled) return;
  if(sub!=NULL){
    NB_TreePath treePath;
    NB_TreeNode *treeNode;
    treeNode=(NB_TreeNode *)nbTreeLocate(&treePath,sub,(NB_TreeNode **)&pub->sub);
    if(treeNode!=NULL){
      nbTreeRemove(&treePath);
      nbFree((NB_Object *)treeNode,sizeof(NB_TreeNode));  // this should be a macro
      }
    }
  if(pub->sub==NULL){
    pub->object.type->disable(pub);
    /* We make an exception for terms who stay enabled when their defined
    *  to have the value of a static object (not variable).  Perhaps we
    *  should leave it up to the disable functions, but all except NB_Term
    *  are currently depending on nbAxonDisable() to turn the lights out.
    */
    if(pub->object.type!=termType){
      dropObject(pub->object.value);
      pub->object.value=nb_Disabled;
      }
    }
  if(trace) outMsg(0,'T',"nbAxonDisable() returning");
  }

/*
*  Enable an axon cell
*  The axon subscription tree is organized by constant value instead of by subscriber address.
*/
static void nbAxonEnableBoost(NB_Cell *pub,NB_Cell *sub){
  NB_TreePath treePath;
  NB_TreeNode *treeNode;
  //if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish

  if(sub->object.type==condTypeRelEQ || sub->object.type==condTypeRelNE)
    treeNode=(NB_TreeNode *)nbTreeLocateCondRight(&treePath,((NB_Cond *)sub)->right,(NB_TreeNode **)&pub->sub);
  else if(sub->object.type->attributes&TYPE_IS_REL){
    if(((NB_Object *)((NB_Cond *)sub)->right)->type==strType)
      treeNode=(NB_TreeNode *)nbTreeLocateCondRightString(&treePath,((NB_String *)((NB_Cond *)sub)->right)->value,(NB_TreeNode **)&pub->sub);
    else if(((NB_Object *)((NB_Cond *)sub)->right)->type==realType)
      treeNode=(NB_TreeNode *)nbTreeLocateCondRightReal(&treePath,((NB_Real *)((NB_Cond *)sub)->right)->value,(NB_TreeNode **)&pub->sub);
    else{
      outMsg(0,'L',"nbAxonEnableBoost: called with unsupported subscriber type - expecting string or real constant on right");
      return;
      }
    }
  else{
    outMsg(0,'L',"nbAxonEnableBoost: called with unsupported subscriber type - expecting relational operator");
    return;
    }
  if(treeNode==NULL){  // if not already a subscriber, then subscribe
    treeNode=(NB_TreeNode *)nbAlloc(sizeof(NB_TreeNode));
    treePath.key=sub;
    nbTreeInsert(&treePath,treeNode);
    }
  pub->object.value=nb_Unknown; // don't call axon cell eval method when enabling
  if(sub->level<=pub->level){
    ((NB_Cell *)sub)->level=pub->level+1;
    nbCellLevel((NB_Cell *)sub);
    }
  }

static void nbAxonDisableBoost(NB_Cell *pub,NB_Cell *sub){
  if(trace){
    outMsg(0,'T',"nbAxonDisable() called");
    printObject((NB_Object *)pub);
    outPut("\n");
    }
  if(pub->object.value==(NB_Object *)pub) return; /* static object */
  if(pub->object.value==nb_Disabled) return;
  if(sub!=NULL){
    NB_TreePath treePath;
    NB_TreeNode *treeNode;
    if(sub->object.type==condTypeRelEQ || sub->object.type==condTypeRelNE)
      treeNode=(NB_TreeNode *)nbTreeLocateCondRight(&treePath,((NB_Cond *)sub)->right,(NB_TreeNode **)&pub->sub);
    else if(sub->object.type->attributes&TYPE_IS_REL){
      if(((NB_Object *)((NB_Cond *)sub)->right)->type==strType)
        treeNode=(NB_TreeNode *)nbTreeLocateCondRightString(&treePath,((NB_String *)((NB_Cond *)sub)->right)->value,(NB_TreeNode **)&pub->sub);
      else if(((NB_Object *)((NB_Cond *)sub)->right)->type==realType)
        treeNode=(NB_TreeNode *)nbTreeLocateCondRightReal(&treePath,((NB_Real *)((NB_Cond *)sub)->right)->value,(NB_TreeNode **)&pub->sub);
      else{
        outMsg(0,'L',"nbAxonDisableBoost: called with unsupported subscriber type - expecting string or real constant on right");
        return;
        }
      }
    else{
      outMsg(0,'L',"nbAxonDisableBoost: called with unsupported subscriber type - expecting relational operator");
      return;
      }
    if(treeNode!=NULL){
      nbTreeRemove(&treePath);
      nbFree((NB_Object *)treeNode,sizeof(NB_TreeNode));  // this should be a macro
      }
    }
  if(pub->sub==NULL){
    pub->object.type->disable(pub);
    /* We make an exception for terms who stay enabled when their defined
    *  to have the value of a static object (not variable).  Perhaps we
    *  should leave it up to the disable functions, but all except NB_Term
    *  are currently depending on nbAxonDisable() to turn the lights out.
    */
    if(pub->object.type!=termType){
      dropObject(pub->object.value);
      pub->object.value=nb_Disabled;
      }
    }
  if(trace) outMsg(0,'T',"nbAxonDisable() returning");
  }

/*
*  Enable and test to see if an axon accelerator will help
*
*  This function is not pleasing.  It requires rework to avoid checking
*  for specific condition types.  It may be appropriate to include an object
*  type method for enabling an axon accelerator cell.  It may also be appropriate
*  to include a cell mode flag indicating which cells can use a booster.  For
*  example, not all relational conditions of a given type can make use of the
*  available booster.  It would be better to figure that out once when the cell
*  is created so we don't have to figure that out again in this routine when
*  traversing the axon subscription tree.
*/
static NB_AxonRel *nbAxonEnableBoostMaybe(NB_Cell *pub,NB_Cond *cond,NB_Type *type){
  NB_AxonRel *axon=NULL;
  NB_TreePath treePath;
  NB_TreeNode *treeNode;

  treeNode=(NB_TreeNode *)nbTreeLocate(&treePath,cond,(NB_TreeNode **)&pub->sub);
  if(treeNode==NULL){  // if not already a subscriber, then subscribe
    if(treePath.depth<5){
      treeNode=(NB_TreeNode *)nbAlloc(sizeof(NB_TreeNode));
      treeNode->key=cond;
      nbTreeInsert(&treePath,treeNode);
      }
    else{   // We have enough subscriptions to start using accelerator cells
      NB_TreeIterator treeIterator;
      NB_Cell *cell[32];
      NB_Cond *otherCond;
      int c=0;
      axon=useAxonRel(type,(NB_Cell *)cond->left);
      if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
        axon->cell.level=pub->level+1;
        nbAxonEnable(pub,(NB_Cell *)axon);
        }
      nbAxonEnableBoost((NB_Cell *)axon,(NB_Cell *)cond);
      // Create an array of all the cells for which an accelerator can help
      NB_TREE_ITERATE(treeIterator,treeNode,pub->sub){
        otherCond=(NB_Cond *)treeNode->key;
        if(otherCond->cell.object.type->attributes&TYPE_IS_REL && 
           ((NB_Object *)otherCond->right)->value!=nb_Unknown &&
           ((NB_Object *)otherCond->right)->value==otherCond->right &&
           ((NB_Object *)otherCond->left)->value!=otherCond->left){
          cell[c]=(NB_Cell *)treeNode->key;
          c++;
          }
        NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
        }
      pub->mode|=NB_CELL_MODE_AXON_BOOST; // set flag so future subscriptions use accelerator
      for(c--;c>=0;c--){
        nbAxonDisable(pub,cell[c]);
        if(cell[c]->object.type==condTypeRelEQ) nbAxonEnableRelEq(pub,(NB_Cond *)cell[c]);
        else if(cell[c]->object.type==condTypeRelNE) nbAxonEnableRelNe(pub,(NB_Cond *)cell[c]);
        else nbAxonEnableRelRange(pub,(NB_Cond *)cell[c]);
        }
      } 
    }
  if(pub->object.value!=nb_Disabled) return(axon); // already know the value
  pub->object.type->enable(pub); /* pub's enable method */
  pub->object.value=(NB_Object *)grabObject(pub->object.type->eval(pub));  /* pub's evaluation method */
  return(axon);
  }

/*
*  Enable relational equal - insert axon accelerator cell if appropriate
*/
void nbAxonEnableRelEq(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;

  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  if(pub->mode&NB_CELL_MODE_AXON_BOOST){          // accelerate if we already decided to
    axon=useAxonRel(nb_TypeAxonRelEq,(NB_Cell *)cond->left);
    if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
      axon->cell.level=pub->level+1;
      nbAxonEnable(pub,(NB_Cell *)axon);
      }
    nbAxonEnableBoost((NB_Cell *)axon,(NB_Cell *)cond);
    }
  else axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelEq);
  if(axon){
    cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
    if(pub->object.value==nb_Unknown) axon->trueCell=(NB_Cell *)nb_Unknown;
    else if(cond->cell.object.value==nb_True) axon->trueCell=(NB_Cell *)cond;
    }
  }

void nbAxonDisableRelEq(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;

  if(!(pub->mode&NB_CELL_MODE_AXON_BOOST)){
    nbAxonDisable(pub,(NB_Cell *)cond);
    return;
    }
  if(pub->object.value==(NB_Object *)pub) return; /* constant object */
  if(pub->object.value==nb_Disabled) return;
  axon=useAxonRel(nb_TypeAxonRelEq,(NB_Cell *)cond->left);
  if(axon->trueCell==(NB_Cell *)cond) axon->trueCell=NULL;
  nbAxonDisableBoost((NB_Cell *)axon,(NB_Cell *)cond);
  if(axon->cell.sub==NULL){ // if we now have no subscribers
    nbAxonDisable(pub,(NB_Cell *)axon);
    destroyAxonRel(axon);
    }
  }

void nbAxonEnableRelNe(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;

  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  if(pub->mode&NB_CELL_MODE_AXON_BOOST){          // accelerate if we already decided to
    axon=useAxonRel(nb_TypeAxonRelNe,(NB_Cell *)cond->left);
    if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
      axon->cell.level=pub->level+1;
      nbAxonEnable(pub,(NB_Cell *)axon);
      }
    nbAxonEnableBoost((NB_Cell *)axon,(NB_Cell *)cond);
    }
  else axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelNe);
  if(axon){
    cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
    if(cond->cell.object.value==nb_False) axon->falseCell=(NB_Cell *)cond;
    if(pub->object.value==nb_Unknown) axon->falseCell=(NB_Cell *)nb_Unknown;
    }
  }

void nbAxonDisableRelNe(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;

  if(!(pub->mode&NB_CELL_MODE_AXON_BOOST)){
    nbAxonDisable(pub,(NB_Cell *)cond);
    return;
    }
  if(pub->object.value==(NB_Object *)pub) return; /* constant object */
  if(pub->object.value==nb_Disabled) return;
  axon=useAxonRel(nb_TypeAxonRelNe,(NB_Cell *)cond->left);
  if(axon->trueCell==(NB_Cell *)cond) axon->trueCell=NULL;
  nbAxonDisableBoost((NB_Cell *)axon,(NB_Cell *)cond);
  if(axon->cell.sub==NULL){ // if we now have no subscribers
    nbAxonDisable(pub,(NB_Cell *)axon);
    destroyAxonRel(axon);
    }
  }

// 2013-12-19 eat - experimenting with type specific axon cells
// NOTE: The axon booster cell type can be added as an attribute of a type
//       to eleminate all the checking in this function, although this would
//       require both a string an real booster.
void nbAxonEnableRelRange(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;
  NB_Type *condType,*valType;

  if(pub->object.value==(NB_Object *)pub) return;  // constant object doesn't publish
  condType=cond->cell.object.type;
  valType=((NB_Object *)cond->right)->type;
  if(pub->mode&NB_CELL_MODE_AXON_BOOST){          // accelerate if we already decided to
    if(valType==strType){
      if(condType==condTypeRelLT)      axon=useAxonRel(nb_TypeAxonRelLtString,(NB_Cell *)cond->left);
      else if(condType==condTypeRelLE) axon=useAxonRel(nb_TypeAxonRelLeString,(NB_Cell *)cond->left);
      else if(condType==condTypeRelGT) axon=useAxonRel(nb_TypeAxonRelGtString,(NB_Cell *)cond->left);
      else if(condType==condTypeRelGE) axon=useAxonRel(nb_TypeAxonRelGeString,(NB_Cell *)cond->left);
      else{
        outMsg(0,'L',"nbAxonEnaleRelRange: called for unrecognized condition");
        return;
        }
     }
    else if(valType==realType){
      if(condType==condTypeRelLT)      axon=useAxonRel(nb_TypeAxonRelLtReal,(NB_Cell *)cond->left);
      else if(condType==condTypeRelLE) axon=useAxonRel(nb_TypeAxonRelLeReal,(NB_Cell *)cond->left);
      else if(condType==condTypeRelGT) axon=useAxonRel(nb_TypeAxonRelGtReal,(NB_Cell *)cond->left);
      else if(condType==condTypeRelGE) axon=useAxonRel(nb_TypeAxonRelGeReal,(NB_Cell *)cond->left);
      else{
        outMsg(0,'L',"nbAxonEnaleRelRange: called for unrecognized condition");
        return;
        }
     }
    else{
      outMsg(0,'L',"nbAxonEnableRelRange: called for condition with right side not real or string");
      return;
      }
    if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
      axon->cell.level=pub->level+1;
      nbAxonEnable(pub,(NB_Cell *)axon);
      }
    nbAxonEnableBoost((NB_Cell *)axon,(NB_Cell *)cond);
    }
  else{
    //outMsg(0,'T',"nbAxonEnableRelRange: called");
    //printObject((NB_Object *)cond);
      //outPut("\n");
    // 2013-12-19 eat - make the axon cell type specific for efficiency in comparisons
    if(valType==strType){
      if(condType==condTypeRelLT)      axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelLtString);
      else if(condType==condTypeRelLE) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelLeString);
      else if(condType==condTypeRelGT) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelGtString);
      else if(condType==condTypeRelGE) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelGeString);
      else{
        outMsg(0,'L',"nbAxonEnaleRelRange: called for unrecognized condition");
        return;
        }
      }
    else if(valType==realType){
      if(condType==condTypeRelLT)      axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelLtReal);
      else if(condType==condTypeRelLE) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelLeReal);
      else if(condType==condTypeRelGT) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelGtReal);
      else if(condType==condTypeRelGE) axon=nbAxonEnableBoostMaybe(pub,cond,nb_TypeAxonRelGeReal);
      else{
        outMsg(0,'L',"nbAxonEnableRelRange: called for unrecognized condition");
        return;
        }
      }
    else{
      outMsg(0,'L',"nbAxonEnableRelRange: called for condition with right side not real or string");
      return;
      }
    }
  if(axon){
    //cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
    outMsg(0,'T',"nbAxonEnableRelRange: pub %p",pub);
    outMsg(0,'T',"nbAxonEnableRelRange: pub %p pub->object.value %p",pub,pub->object.value);
    outMsg(0,'T',"nbAxonEnableRelRange: pub %p pub->object.value %p pub->object.value->type %p",pub,pub->object.value,pub->object.value->type);
    if(pub->object.value->type==valType)  
      axon->value=grabObject(pub->object.value);
    else axon->value=NULL;
    }
  }

void nbAxonDisableRelRange(NB_Cell *pub,NB_Cond *cond){
  NB_AxonRel *axon;
  NB_Type *condType,*valType;

  if(!(pub->mode&NB_CELL_MODE_AXON_BOOST)){
    nbAxonDisable(pub,(NB_Cell *)cond);
    return;
    }
  if(pub->object.value==(NB_Object *)pub) return;  // constant object doesn't publish
  //outMsg(0,'T',"nbAxonDisableRelRange: called");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  condType=cond->cell.object.type;
  valType=((NB_Object *)cond->right)->type;
  if(valType==strType){
    if(condType==condTypeRelLT)      axon=useAxonRel(nb_TypeAxonRelLtString,(NB_Cell *)cond->left);
    else if(condType==condTypeRelLE) axon=useAxonRel(nb_TypeAxonRelLeString,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGT) axon=useAxonRel(nb_TypeAxonRelGtString,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGE) axon=useAxonRel(nb_TypeAxonRelGeString,(NB_Cell *)cond->left);
    else{
      outMsg(0,'L',"nbAxonDisableRelRange: called for unrecognized condition");
      return;
      }
    }
  else if(valType==realType){
    if(condType==condTypeRelLT)      axon=useAxonRel(nb_TypeAxonRelLtReal,(NB_Cell *)cond->left);
    else if(condType==condTypeRelLE) axon=useAxonRel(nb_TypeAxonRelLeReal,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGT) axon=useAxonRel(nb_TypeAxonRelGtReal,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGE) axon=useAxonRel(nb_TypeAxonRelGeReal,(NB_Cell *)cond->left);
    else{
      outMsg(0,'L',"nbAxonDisableRelRange: called for unrecognized condition");
      return;
      }
    }
  else{
    outMsg(0,'L',"nbAxonDisableRelRange: called for condition with right side not real or string");
    return;
    }
  if(axon->trueCell==(NB_Cell *)cond) axon->trueCell=NULL;
  nbAxonDisableBoost((NB_Cell *)axon,(NB_Cell *)cond);
  if(axon->cell.sub==NULL){ // if we now have no subscribers
    nbAxonDisable(pub,(NB_Cell *)axon);
    destroyAxonRel(axon);
    }
  }

/*
*  Publish change to all subscriber objects via their alert method
*/
void nbAxonAlert(NB_Cell *pub){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  if(trace){
    outMsg(0,'T',"nbCellPublish() called for object %p:",pub);
    printObject((NB_Object *)pub);
    outPut("\n");
    outFlush();
    }
  if(pub->object.value==(NB_Object *)pub) return; /* static object */
  NB_TREE_ITERATE(treeIterator,treeNode,pub->sub){
    ((NB_Object *)treeNode->key)->type->alert((NB_Cell *)treeNode->key); /* could be any object */
    NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
    }
  if(trace){
    outMsg(0,'T',"nbCellPublish() returning for:");
    printObject((NB_Object *)pub);
    outPut("\n");
    }
  }

