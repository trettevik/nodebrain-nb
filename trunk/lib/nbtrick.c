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
* File:     nbtrick.c 
*
* Title:    Trick Cell Functions 
*
* Function:
*
*   This file provides methods for nodebrain trick cells that provide
*   accelerated cell evaluation for some class of cells by use of a
*   reduction technique.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initTrick();
* 
* Description
*
*   A trick cell subscribes to a cell on behalf of multiple dependent
*   cells.  The trick cell manages the evaluation of the dependent cells,
*   all of which qualify for a technique that can reduce the number of
*   evaluations.
*
*   The subscription tree for a trick cell is ordered by value instead of
*   address.  The ordering routine is provided by the trick.  It is because
*   of the ordering that the trick cell is able to "know" that a subset
*   of the subscribers do not need to be evaluated because their value
*   would not change.
*
*   Trick objects implemented in this file may be closely related to operations
*   implemented in nbcondition.c.  We have split the trick routines out
*   to avoid cluttering nbcondition.c
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2013-12-09 Ed Trettevik (original version introduced in 0.9.0)
*=============================================================================
*/
#include <nb/nbi.h>

static struct HASH *trickH=NULL;

struct TYPE *nb_TypeTrickRelEq=NULL;
struct TYPE *nb_TypeTrickRelLtString=NULL;
struct TYPE *nb_TypeTrickRelLtReal=NULL;
struct TYPE *nb_TypeTrickRelGtString=NULL;
struct TYPE *nb_TypeTrickRelGtReal=NULL;

/*
*  Hash a trick and return a pointer to a pointer in the hashing vector.
*/
static void *hashTrickRelEq(struct HASH *hash,struct TYPE *type,void *pub){
  long h,*t,*p;
  //printf("hashTrick: type=%p pub=%p sizeof(long)=%ld sizeof(void *)=%ld\n",type,pub,sizeof(h),sizeof(void *));
  t=(long *)&type;
  p=(long *)&pub;
  h=(*p & 0x0fffffff) + (*t & 0x0fffffff);
  //printf("hashTrick: n=%ld modulo=%ld index=%ld\n",h,hash->modulo,h%hash->modulo);
  return(&(hash->vect[h%hash->modulo]));
  }

static NB_TrickRelEq *useTrickRelEq(struct TYPE *type,NB_Cell *pub){
  NB_TrickRelEq *trick,**trickP;

  trickP=hashTrickRelEq(trickH,type,pub);
  for(trick=*trickP;trick!=NULL;trick=*trickP){
    if(trick->pub==pub && trick->cell.object.type==type) return(trick);
    trickP=(NB_TrickRelEq **)&trick->cell.object.next;
    }
  trick=(NB_TrickRelEq *)nbCellNew(type,NULL,sizeof(NB_TrickRelEq));
  trick->cell.object.next=(NB_Object *)*trickP;
  *trickP=trick; 
  trick->pub=pub;
  return(trick);
  }

// 2013-13-19 eat - this is a stub - need to modify for range object
static NB_TrickRelEq *useTrickRelRange(struct TYPE *type,NB_Cell *pub){
  NB_TrickRelEq *trick,**trickP;

  trickP=hashTrickRelEq(trickH,type,pub);
  for(trick=*trickP;trick!=NULL;trick=*trickP){
    if(trick->pub==pub && trick->cell.object.type==type) return(trick);
    trickP=(NB_TrickRelEq **)&trick->cell.object.next;
    }
  trick=(NB_TrickRelEq *)nbCellNew(type,NULL,sizeof(NB_TrickRelEq));
  trick->cell.object.next=(NB_Object *)*trickP;
  *trickP=trick;
  trick->pub=pub;
  return(trick);
  }

/*
*  Compare two object by RelEq conditions (when known not to be Unknown)
*
*  Return:
*     -1 c1<c2
*      0 c1=c2 string, real, list, set (we don't care what object type)
*      1 c1>c2
*/
static int compareTrickRelEq(void *handle,void *c1,void *c2){
  if(((NB_Cond *)c1)->right<((NB_Cond *)c2)->right) return(-1);
  else if(((NB_Cond *)c1)->right>((NB_Cond *)c2)->right) return(1);
  return(0);
  }

/*
*  Compare two string constants pointed to by RelLt or RelGt conditions
*
*  Return:
*     -1 c1<c2
*      0 c1=c2 string=string
*      1 c1>c2
*/
static int compareTrickRelString(void *handle,void *c1,void *c2){
  char *c1Str,*c2Str;

  c1Str=((NB_String *)((NB_Cond *)c1)->right)->value;
  c2Str=((NB_String *)((NB_Cond *)c2)->right)->value;
  return(strcmp(c1Str,c2Str));
  }

/*
*  Compare two number constants pointed to by RelLt or RelGt conditions
*
*  Return:
*     -1 c1<c2
*      0 c1=c2 real=real
*      1 c1>c2
*/
static int compareTrickRelReal(void *handle,void *c1,void *c2){
  double c1Real,c2Real;

  c1Real=((NB_Real *)((NB_Cond *)c1)->right)->value;
  c2Real=((NB_Real *)((NB_Cond *)c2)->right)->value;
  if(c1Real<c2Real) return(-1);
  else if(c1Real==c2Real) return(0);
  else return(1);
  }

/**********************************************************************
* Private Object Methods
**********************************************************************/
static void printTrick(struct NB_TRICK_REL_EQ *trick){
  if(trick==NULL) outPut("(?)");
  else{
    outPut("%s(",trick->cell.object.type->name);
    printObject((NB_Object *)trick->pub);
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
static NB_Object *evalTrickRelEq(NB_TrickRelEq *trick){
  NB_TreeNode *treeNode;
  NB_Cond cond;      // condition to compare to subscribers
  NB_Cell *trueCell;  // condition that is true

  // Search for subscriber with right operand equal to new value of pub
  cond.cell.object.type=condTypeRelEQ; // just to make it a bit legit
  cond.left=nb_Unknown;
  cond.right=trick->pub->object.value;
  if(cond.right==nb_Unknown){
    nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    trick->trueCell=(NB_Cell *)nb_Unknown;
    }
  else if(trick->trueCell==(NB_Cell *)nb_Unknown){
    nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate
    treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelEq,NULL);
    if(treeNode) trick->trueCell=(NB_Cell *)treeNode->key;
    else trick->trueCell=NULL;
    }
  else if(NULL!=(treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelEq,NULL))){
    trueCell=(NB_Cell *)treeNode->key;
    if(trueCell!=trick->trueCell){ // This should always be true
      trueCell->object.value=NB_OBJECT_TRUE;
      nbCellPublish(trueCell);
      if(trick->trueCell){
        trick->trueCell->object.value=nb_False;
        nbCellPublish(trick->trueCell);
        }
      trick->trueCell=trueCell;
      }
    }
  else if(trick->trueCell){  // we have no match
    trick->trueCell->object.value=nb_False;
    nbCellPublish(trick->trueCell);
    trick->trueCell=NULL;
    }
  //outMsg(0,'T',"evalTrickRelEq: returning");
  return(nb_Unknown);
  }


/*
*  Eval Rel Eq 
*
*    Evaluate 1 or 2 of the potentially many subscribers
*/
static NB_Object *evalTrickRelRange(NB_TrickRelEq *trick){
  NB_TreeNode *treeNode;
  NB_Cond cond;      // condition to compare to subscribers
  NB_Cell *trueCell;  // condition that is true

  // Search for subscriber with right operand equal to new value of pub
  cond.cell.object.type=condTypeRelEQ; // just to make it a bit legit
  cond.left=nb_Unknown;
  cond.right=trick->pub->object.value;
  if(trick->cell.object.type==nb_TypeTrickRelLtReal && ((NB_Object *)cond.right)->type==realType){
    if(trick->trueCell==(NB_Cell *)nb_Unknown){
      nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate
      treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelReal,NULL);
      if(treeNode) trick->trueCell=(NB_Cell *)treeNode->key;
      else trick->trueCell=NULL;
      return(nb_Unknown);
      }
    treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelReal,NULL);
    }
  else if(trick->cell.object.type==nb_TypeTrickRelLtString && ((NB_Object *)cond.right)->type==strType){
    if(trick->trueCell==(NB_Cell *)nb_Unknown){
      nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate
      treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelString,NULL);
      if(treeNode) trick->trueCell=(NB_Cell *)treeNode->key;
      else trick->trueCell=NULL;
      return(nb_Unknown);
      }
    treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelString,NULL);
    }
  else treeNode=NULL;
  if(treeNode){
    trueCell=(NB_Cell *)treeNode->key;
    if(trueCell!=trick->trueCell){
      trueCell->object.value=NB_OBJECT_TRUE;
      nbCellPublish(trueCell);
      if(trick->trueCell){
        trick->trueCell->object.value=nb_False;
        nbCellPublish(trick->trueCell);
        }
      trick->trueCell=trueCell;
      }
    else if(trick->trueCell){
      trick->trueCell->object.value=nb_False;
      nbCellPublish(trick->trueCell);
      trick->trueCell=NULL;
      }
    }
  else if(cond.right==nb_Unknown){
    nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    trick->trueCell=(NB_Cell *)nb_Unknown;
    }
  else if(trick->trueCell==(NB_Cell *)nb_Unknown) nbCellPublish((NB_Cell *)trick); // force evaluation
  else if(trick->trueCell){  // we have no match
    trick->trueCell->object.value=nb_False;
    nbCellPublish(trick->trueCell);
    trick->trueCell=NULL;
    }
  //outMsg(0,'T',"evalTrickRelEq: returning");
  return(nb_Unknown);
  }

/*
*  Solve Trick - dummy function to let us know if it is attempted
*/
static void solveTrick(struct CALL *call){
  outMsg(0,'L',"Logic error - solveTrickRelEq called - trick cells should not be referenced like this");
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
static void enableTrickRel(NB_TrickRelEq *trick){   
  //outMsg(0,'T',"enableTrickRelEq: called");
  }

static void disableTrickRel(NB_TrickRelEq *trick){
  //outMsg(0,'T',"disableTrickRelEq: called");
  }

static void destroyTrickRelEq(NB_TrickRelEq *trick){
  NB_TrickRelEq *ltrick,**trickP;

  nbCellDisable((NB_Cell *)trick->pub,(NB_Cell *)trick); // should we check first
  dropObject(trick->pub);

  trickP=hashTrickRelEq(trickH,trick->cell.object.type,trick->pub);
  for(ltrick=*trickP;ltrick!=NULL && ltrick!=trick;ltrick=*trickP)
    trickP=(NB_TrickRelEq **)&ltrick->cell.object.next;
  if(ltrick) *trickP=(NB_TrickRelEq *)trick->cell.object.next;
  nbFree(trick,sizeof(NB_TrickRelEq)); 
  }

// 2013-12-19 eat - this is a stub - modify to destroy range trick cell
static void destroyTrickRelRange(NB_TrickRelEq *trick){
  NB_TrickRelEq *ltrick,**trickP;

  nbCellDisable((NB_Cell *)trick->pub,(NB_Cell *)trick); // should we check first
  dropObject(trick->pub);

  trickP=hashTrickRelEq(trickH,trick->cell.object.type,trick->pub);
  for(ltrick=*trickP;ltrick!=NULL && ltrick!=trick;ltrick=*trickP)
    trickP=(NB_TrickRelEq **)&ltrick->cell.object.next;
  if(ltrick) *trickP=(NB_TrickRelEq *)trick->cell.object.next;
  nbFree(trick,sizeof(NB_TrickRelEq));
  }


/**********************************************************************
* Public Methods
**********************************************************************/
void initTrick(NB_Stem *stem){
  //outMsg(0,'T',"initTrick: called");
  trickH=newHash(100003);  /* initialize condition hash */
  nb_TypeTrickRelEq=newType(stem,"AxonRelEq",NULL,0,printTrick,destroyTrickRelEq);
  nbCellType(nb_TypeTrickRelEq,solveTrick,evalTrickRelEq,enableTrickRel,disableTrickRel);
  nb_TypeTrickRelLtString=newType(stem,"AxonRelLtString",NULL,0,printTrick,destroyTrickRelEq);
  nbCellType(nb_TypeTrickRelLtString,solveTrick,evalTrickRelRange,enableTrickRel,disableTrickRel);
  nb_TypeTrickRelLtReal=newType(stem,"AxonRelLtReal",NULL,0,printTrick,destroyTrickRelEq);
  nbCellType(nb_TypeTrickRelLtReal,solveTrick,evalTrickRelRange,enableTrickRel,disableTrickRel);
  nb_TypeTrickRelGtString=newType(stem,"AxonRelLtString",NULL,0,printTrick,destroyTrickRelRange);
  nbCellType(nb_TypeTrickRelGtString,solveTrick,evalTrickRelRange,enableTrickRel,disableTrickRel);
  nb_TypeTrickRelGtReal=newType(stem,"AxonRelLtReal",NULL,0,printTrick,destroyTrickRelRange);
  nbCellType(nb_TypeTrickRelGtReal,solveTrick,evalTrickRelRange,enableTrickRel,disableTrickRel);
  //outMsg(0,'T',"initTrick: returning");
  }

// 2013-12-19 eat - experimenting with type specific trick cells
void nbTrickRelEqEnable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  // 2013-12-19 eat - make the trick cell type specific for efficiency in comparisons
  trick=useTrickRelEq(nb_TypeTrickRelEq,(NB_Cell *)cond->left);
  if(trick->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
    trick->cell.level=pub->level+1;
    nbCellEnable(pub,(NB_Cell *)trick); 
    }
  nbCellEnableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelEq,NULL);
  cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
  if(cond->cell.object.value==NB_OBJECT_TRUE) trick->trueCell=(NB_Cell *)cond;
  if(pub->object.value==nb_Unknown) trick->trueCell=(NB_Cell *)nb_Unknown;
  }

// 2013-12-19 eat - experimenting with type specific trick cells
void nbTrickRelRangeEnable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  if(pub->object.value==(NB_Object *)pub) return;  // simple object doesn't publish
  //outMsg(0,'T',"nbTrickRelEqEnable: called");
  //outPut("pub:");
  //printObject((NB_Object *)pub);
  //outPut("\nsub:");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  // 2013-12-19 eat - make the trick cell type specific for efficiency in comparisons
  if(((NB_Object *)cond->right)->type==strType)
    trick=useTrickRelRange(nb_TypeTrickRelLtString,(NB_Cell *)cond->left);
  else trick=useTrickRelRange(nb_TypeTrickRelLtReal,(NB_Cell *)cond->left);
  //outMsg(0,'T',"nbTrickRelEqEnable: 1");
  if(trick->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
    trick->cell.level=pub->level+1;
    nbCellEnable(pub,(NB_Cell *)trick);
    }
  //outMsg(0,'T',"nbTrickRelEqEnable: 2 trick->level=%d",trick->cell.level);
  if(((NB_Object *)cond->right)->type==strType)
    nbCellEnableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelString,NULL);
  else nbCellEnableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelReal,NULL);
  cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
  if(cond->cell.object.value==NB_OBJECT_TRUE) trick->trueCell=(NB_Cell *)cond;
  if(pub->object.value==nb_Unknown) trick->trueCell=(NB_Cell *)nb_Unknown;

  // the next line will change when we are ready
  //nbCellEnable(pub,(NB_Cell *)cond);
  //outMsg(0,'T',"nbTrickRelEqEnable: returning");
  }

void nbTrickRelEqDisable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  if(pub->object.value==(NB_Object *)pub) return; /* static object */
  if(pub->object.value==nb_Disabled) return;

  //outMsg(0,'T',"nbTrickRelEqDisable: called");
  //outPut("pub:");
  //printObject((NB_Object *)pub);
  //outPut("\nsub:");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  //outFlush();
  trick=useTrickRelEq(nb_TypeTrickRelEq,(NB_Cell *)cond->left);
  if(trick->trueCell==(NB_Cell *)cond) trick->trueCell=NULL;
  nbCellDisableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelEq,NULL);
  if(trick->cell.sub==NULL){ // if we now have no subscribers
    nbCellDisable(pub,(NB_Cell *)trick);
    destroyTrickRelEq(trick);
    }
  //outMsg(0,'T',"nbTrickRelEqDisable: returning");
  }

void nbTrickRelRangeDisable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  if(pub->object.value==(NB_Object *)pub) return; /* static object */
  if(pub->object.value==nb_Disabled) return;

  //outMsg(0,'T',"nbTrickRelEqDisable: called");
  //outPut("pub:");
  //printObject((NB_Object *)pub);
  //outPut("\nsub:");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  //outFlush();
  if(((NB_Object *)cond->right)->type==strType)
    trick=useTrickRelRange(nb_TypeTrickRelLtString,(NB_Cell *)cond->left);
  else trick=useTrickRelRange(nb_TypeTrickRelLtReal,(NB_Cell *)cond->left);
  if(trick->trueCell==(NB_Cell *)cond) trick->trueCell=NULL;
  if(((NB_Object *)cond->right)->type==strType)
    nbCellDisableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelString,NULL);
  else nbCellDisableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelReal,NULL);
  if(trick->cell.sub==NULL){ // if we now have no subscribers
    nbCellDisable(pub,(NB_Cell *)trick);
    destroyTrickRelEq(trick);
    }
  //outMsg(0,'T',"nbTrickRelEqDisable: returning");
  }

