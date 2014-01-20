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
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *nb_TypeAxonRelEq=NULL;
struct TYPE *nb_TypeAxonRelLtString=NULL;
struct TYPE *nb_TypeAxonRelLtReal=NULL;
struct TYPE *nb_TypeAxonRelGtString=NULL;
struct TYPE *nb_TypeAxonRelGtReal=NULL;

/*
*  Hash a axon and return a pointer to a pointer in the hashing vector.
*/
/*
static uint32_t hashAxonRelEq(struct TYPE *type,void *pub){
  //long h,*t,*p;
  long h,*p;
  uint32_t key;
  //printf("hashAxon: type=%p pub=%p sizeof(long)=%ld sizeof(void *)=%ld\n",type,pub,sizeof(h),sizeof(void *));
  //t=(long *)&type;
  p=(long *)&pub;
  //h=(*p & 0x0fffffff) + (*t & 0x0fffffff);
  h=(*p>>3);
  key=h;
  //printf("hashAxon: n=%ld modulo=%ld index=%ld\n",h,hash->modulo,h%hash->modulo);
  return(key);
  }
*/

static NB_AxonRel *useAxonRel(NB_Type *type,NB_Cell *pub){
  NB_AxonRel *axon,**axonP;
  NB_Hash *hash=type->hash;
  uint32_t key;

  //key=hashAxonRelEq(type,pub);
  key=pub->object.key;  // we can use the publishers key once they all have one
  axonP=(NB_AxonRel **)&(hash->vect[key%hash->modulo]);
  for(axon=*axonP;axon!=NULL;axon=*axonP){
    if(axon->pub==pub && axon->cell.object.type==type) return(axon);
    axonP=(NB_AxonRel **)&axon->cell.object.next;
    }
  axon=(NB_AxonRel *)nbCellNew(type,NULL,sizeof(NB_AxonRel));
  axon->cell.object.key=key;
  axon->cell.object.next=(NB_Object *)*axonP;
  *axonP=axon; 
  hash->objects++;
  if(hash->objects>=hash->modulo) nbHashGrow(&type->hash);
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

  //outMsg(0,'T',"evalAxonRelEq: called");
  // Search for subscriber with right operand equal to new value of pub
  if(right==nb_Unknown){
    //outMsg(0,'T',"evalAxonRelEq: right is Unknown");
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    axon->trueCell=(NB_Cell *)nb_Unknown;
    }
  else if(axon->trueCell==(NB_Cell *)nb_Unknown){
    //outMsg(0,'T',"evalAxonRelEq: trueCell is Unknown");
    nbCellPublish((NB_Cell *)axon); // force all subscribers to evaluate
    //treeNode=nbTreeFindCondRight(right,axon->cell.sub);
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode) axon->trueCell=(NB_Cell *)treeNode->key;
    else axon->trueCell=NULL;
    }
  else{
    // if(NULL!=(treeNode=nbTreeFindCondRight(right,axon->cell.sub))){
    NB_TREE_FIND_COND_RIGHT(right,treeNode)
    if(treeNode){
      //outMsg(0,'T',"evalAxonRelEq: found COND");
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
      //outMsg(0,'T',"evalAxonRelEq: COND not found - and we have an old trueCell");
      axon->trueCell->object.value=nb_False;
      nbCellPublish(axon->trueCell);
      axon->trueCell=NULL;
      }
    }
  //else outMsg(0,'T',"evalAxonRelEq: COND not found - and we have no old trueCell");
  //outMsg(0,'T',"evalAxonRelEq: returning");
  return(nb_Unknown);
  }

/*
*  Evaluate axon managed relational condition for real numbers
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
      else condValue=nb_False;
      }
    else{                                             // if prior value was real number greater than new
      max=axon->real->value;
      min=((NB_Real *)value)->value;
      if(axon->cell.object.type==nb_TypeAxonRelLtReal) condValue=nb_True,edge=1;
      else condValue=nb_False;
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

/*
*  Evaluate axon managed relational condition for string values
*/
static NB_Object *evalAxonRelString(NB_AxonRel *axon){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode,*stack[32];
  NB_Object *value;
  NB_Object *condValue;
  char *min,*max;
  int s;
  int edge=0;

  //outMsg(0,'T',"evalAxonRelString: called");
  value=axon->pub->object.value;
  if(value->type==strType){                // if published value is a string
    if(axon->string==NULL){                  // if prior value was not string 
      nbCellPublish((NB_Cell *)axon);
      axon->string=(NB_String *)value;
      grabObject(axon->string);
      return(nb_Unknown);
      }
    if(strcmp(axon->string->value,((NB_String *)value)->value)<1){  // if prior value was string less than new
      min=axon->string->value;
      max=((NB_String *)value)->value;
      if(axon->cell.object.type==nb_TypeAxonRelGtString) condValue=nb_True,edge=-1;
      else condValue=nb_False;
      }
    else{                                             // if prior value was string greater than new
      max=axon->string->value;
      min=((NB_String *)value)->value;
      if(axon->cell.object.type==nb_TypeAxonRelLtString) condValue=nb_True,edge=1;
      else condValue=nb_False;
      }
    // build path to smallest number within range
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
      ((NB_Object *)treeNode->key)->value=condValue;
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
  else if(axon->string){
    //outMsg(0,'T',"evalAxonRelString: setting all subscribing conditions to Unknown");
    // set everything relational cell to unknown
    NB_TREE_ITERATE(treeIterator,treeNode,axon->cell.sub){
      dropObject(((NB_Object *)treeNode->key)->value);
      ((NB_Object *)treeNode->key)->value=nb_Unknown; /* could be any object */
      nbCellPublish((NB_Cell *)treeNode->key);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    dropObject(axon->string);
    axon->string=NULL;
    }
  //outMsg(0,'T',"evalAxonRelString: returning");
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

static void destroyAxonRelEq(NB_AxonRel *axon){
  NB_AxonRel *laxon,**axonP;
  NB_Hash *hash=axon->cell.object.type->hash;
  uint32_t key;

  nbCellDisable((NB_Cell *)axon->pub,(NB_Cell *)axon); // should we check first
  dropObject(axon->pub);

  //key=hashAxonRelEq(axon->cell.object.type,axon->pub);
  key=axon->cell.object.key;
  axonP=(NB_AxonRel **)&(hash->vect[key%hash->modulo]);
  for(laxon=*axonP;laxon!=NULL && laxon!=axon;laxon=*axonP)
    axonP=(NB_AxonRel **)&laxon->cell.object.next;
  if(laxon) *axonP=(NB_AxonRel *)axon->cell.object.next;
  nbFree(axon,sizeof(NB_AxonRel)); 
  hash->objects--;
  }

// 2013-12-19 eat - this is a stub - modify to destroy range axon cell
static void destroyAxonRelRange(NB_AxonRel *axon){
  NB_AxonRel *laxon,**axonP;
  NB_Hash *hash=axon->cell.object.type->hash;
  uint32_t key;

  nbCellDisable((NB_Cell *)axon->pub,(NB_Cell *)axon); // should we check first
  dropObject(axon->pub);

  //key=hashAxonRelEq(axon->cell.object.type,axon->pub);
  key=axon->cell.object.key;
  axonP=(NB_AxonRel **)&(hash->vect[key%hash->modulo]);
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
  //outMsg(0,'T',"initAxon: called");
  nb_TypeAxonRelEq=newType(stem,"AxonRelEq",NULL,0,printAxon,destroyAxonRelEq);
  nbCellType(nb_TypeAxonRelEq,solveAxon,evalAxonRelEq,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLtString=newType(stem,"AxonRelLtString",NULL,0,printAxon,destroyAxonRelEq);
  nbCellType(nb_TypeAxonRelLtString,solveAxon,evalAxonRelString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelLtReal=newType(stem,"AxonRelLtReal",NULL,0,printAxon,destroyAxonRelEq);
  nbCellType(nb_TypeAxonRelLtReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGtString=newType(stem,"AxonRelLtString",NULL,0,printAxon,destroyAxonRelRange);
  nbCellType(nb_TypeAxonRelGtString,solveAxon,evalAxonRelString,enableAxonRel,disableAxonRel);
  nb_TypeAxonRelGtReal=newType(stem,"AxonRelGtReal",NULL,0,printAxon,destroyAxonRelRange);
  nbCellType(nb_TypeAxonRelGtReal,solveAxon,evalAxonRelReal,enableAxonRel,disableAxonRel);
  //outMsg(0,'T',"initAxon: returning");
  }

// 2013-12-19 eat - experimenting with type specific axon cells
void nbAxonRelEqEnable(nbCELL pub,struct COND *cond){
  NB_AxonRel *axon;

  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  // 2013-12-19 eat - make the axon cell type specific for efficiency in comparisons
  axon=useAxonRel(nb_TypeAxonRelEq,(NB_Cell *)cond->left);
  if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
    axon->cell.level=pub->level+1;
    nbCellEnable(pub,(NB_Cell *)axon); 
    }
  nbCellEnableAxon((NB_Cell *)axon,(NB_Cell *)cond);
  cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
  if(cond->cell.object.value==NB_OBJECT_TRUE) axon->trueCell=(NB_Cell *)cond;
  if(pub->object.value==nb_Unknown) axon->trueCell=(NB_Cell *)nb_Unknown;
  }

// 2013-12-19 eat - experimenting with type specific axon cells
void nbAxonRelRangeEnable(nbCELL pub,struct COND *cond){
  NB_AxonRel *axon;
  NB_Type *condType,*valType;

  if(pub->object.value==(NB_Object *)pub) return;  // constant object doesn't publish
  condType=cond->cell.object.type;
  valType=((NB_Object *)cond->right)->type;
  // 2013-12-19 eat - make the axon cell type specific for efficiency in comparisons
  if(valType==strType){
    if(condType==condTypeRelLT)
      axon=useAxonRel(nb_TypeAxonRelLtString,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGT)
      axon=useAxonRel(nb_TypeAxonRelGtString,(NB_Cell *)cond->left);
    else{
      outMsg(0,'L',"nbAxonRelRangeEnable: called for condition not LT or GT");
      return;
      }
    }
  else if(valType==realType){
    if(condType==condTypeRelLT)
      axon=useAxonRel(nb_TypeAxonRelLtReal,(NB_Cell *)cond->left);
    else if(condType==condTypeRelGT)
      axon=useAxonRel(nb_TypeAxonRelGtReal,(NB_Cell *)cond->left);
    else{
      outMsg(0,'L',"nbAxonRelRangeEnable: called for condition not LT or GT");
      return;
      }
    }
  else{
    outMsg(0,'L',"nbAxonRelRangeEnable: called for condition with right side not real or string");
    return;
    }
  if(axon->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
    axon->cell.level=pub->level+1;
    nbCellEnable(pub,(NB_Cell *)axon);
    if(pub->object.value->type==valType)  
      axon->value=grabObject(pub->object.value);
    else axon->value=NULL;
    }
  nbCellEnableAxon((NB_Cell *)axon,(NB_Cell *)cond);
  cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
  }

void nbAxonRelEqDisable(nbCELL pub,struct COND *cond){
  NB_AxonRel *axon;

  if(pub->object.value==(NB_Object *)pub) return; /* constant object */
  if(pub->object.value==nb_Disabled) return;
  axon=useAxonRel(nb_TypeAxonRelEq,(NB_Cell *)cond->left);
  if(axon->trueCell==(NB_Cell *)cond) axon->trueCell=NULL;
  nbCellDisableAxon((NB_Cell *)axon,(NB_Cell *)cond);
  if(axon->cell.sub==NULL){ // if we now have no subscribers
    nbCellDisable(pub,(NB_Cell *)axon);
    destroyAxonRelEq(axon);
    }
  }

void nbAxonRelRangeDisable(nbCELL pub,struct COND *cond){
  NB_AxonRel *axon;

  if(pub->object.value==(NB_Object *)pub) return; /* constant object */
  if(pub->object.value==nb_Disabled) return;
  if(((NB_Object *)cond->right)->type==strType)
    axon=useAxonRel(nb_TypeAxonRelLtString,(NB_Cell *)cond->left);
  else axon=useAxonRel(nb_TypeAxonRelLtReal,(NB_Cell *)cond->left);
  if(axon->trueCell==(NB_Cell *)cond) axon->trueCell=NULL;
  if(((NB_Object *)cond->right)->type==strType)
    nbCellDisableAxon((NB_Cell *)axon,(NB_Cell *)cond);
  else nbCellDisableAxon((NB_Cell *)axon,(NB_Cell *)cond);
  if(axon->cell.sub==NULL){ // if we now have no subscribers
    nbCellDisable(pub,(NB_Cell *)axon);
    destroyAxonRelEq(axon);
    }
  }

