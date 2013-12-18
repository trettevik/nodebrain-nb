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
    if(trick->cell.object.type==type && trick->pub==pub) return(trick);
    trickP=(NB_TrickRelEq **)&trick->cell.object.next;
    }
  trick=(NB_TrickRelEq *)nbCellNew(type,NULL,sizeof(NB_TrickRelEq));
  trick->cell.object.next=(NB_Object *)*trickP;
  *trickP=trick; 
  trick->pub=pub;
  return(trick);
  }

/*
*  Compare two constants pointed to by RelEq conditions
*
*  Return:
*     -3 c1<c2 because c1 is not recognized and c2 is string
*     -2 c1<c2 because c1 is number and c2 isn't
*     -1 c1<c2
*      0 c1=c2 string=string, number=number, or both have unrecognized types
*      1 c1>c2
*      2 c1>c2 because c1 is string and c2 isn't
*      3 c1>c2 because c1 is not recognized and c2 is number
*/
static int compareTrickRelEq(void *handle,void *c1,void *c2){
  NB_Cond *cond1=(NB_Cond *)c1,*cond2=(NB_Cond *)c2;
  int c1Type,c2Type;
  char *c1Str,*c2Str;
  double c1Real,c2Real;
  NB_Object *object1=(NB_Object *)cond1->right,*object2=(NB_Object *)cond2->right;

  if(trace){
    outPut("compareTrickRelEq: comparing ");
    printObject((NB_Object *)cond1);
    outPut(" <==> ");
    printObject((NB_Object *)cond2);
    outPut("\n");
    }
  c1Type=object1->type->apicelltype;
  c2Type=object2->type->apicelltype;
  if(c1Type==NB_TYPE_STRING){
    if(c2Type==NB_TYPE_STRING){
      c1Str=((NB_String *)object1)->value;
      c2Str=((NB_String *)object2)->value;
      return(strcmp(c1Str,c2Str));
      }
    else return(2);
    }
  else if(c1Type==NB_TYPE_REAL){
    if(c2Type==NB_TYPE_REAL){
      c1Real=((NB_Real *)cond1->right)->value;
      c2Real=((NB_Real *)cond2->right)->value;
      if(c1Real<c2Real) return(-1);
      else if(c1Real==c2Real) return(0);
      else return(1);
      }
    else return(-2);
    }
  else{
    if(c2Type==NB_TYPE_STRING) return(3);
    else if(c2Type==NB_TYPE_REAL) return(-3);
    else return(0);
    }
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

  //outMsg(0,'T',"evalTrickRelEq: called");
  // Search for subscriber with right operand equal to new value of pub
  cond.cell.object.type=condTypeRelEQ; // just to make it a bit legit
  cond.left=nb_Unknown;
  cond.right=trick->pub->object.value;
  //outMsg(0,'T',"evalTrickRelEq: calling nbTreeFindValue cond=%p right=%p subroot=%p",cond,cond.right,trick->cell.sub);
  if(cond.right==nb_Unknown){
    //outMsg(0,'T',"evalTrickRelEq: New value is Unknown");
    nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate - replace this with tree traversal setting to unknown and publish subscribers
    trick->trueCell=(NB_Cell *)nb_Unknown;
    }
  else if(trick->trueCell==(NB_Cell *)nb_Unknown){
    //outMsg(0,'T',"evalTrickRelEq: Old value is Unknown");
    nbCellPublish((NB_Cell *)trick); // force all subscribers to evaluate
    treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelEq,NULL);
    if(treeNode) trick->trueCell=(NB_Cell *)treeNode->key;
    else trick->trueCell=NULL;
    }
  else{ // not unknown now, and was't before
    treeNode=nbTreeFindValue(&cond,trick->cell.sub,compareTrickRelEq,NULL);
    if(treeNode){
      //outMsg(0,'T',"evalTrickRelEq: Object %p found in trick %p subscription tree.",cond.right,trick);
      trueCell=(NB_Cell *)treeNode->key; 
      //outMsg(0,'T',"evalTrickRelEq: New true cell cond=%p",trueCell);
      //printObject((NB_Object *)trueCell);
      //outPut("\n");
      if(trueCell!=trick->trueCell){
        trueCell->object.value=NB_OBJECT_TRUE;
        nbCellPublish(trueCell);
        if(trick->trueCell){
          //outMsg(0,'T',"evalTrickRelEq: Old true cell cond=%p",trick->trueCell);
          //printObject((NB_Object *)trick->trueCell);
          //outPut("\n");
          trick->trueCell->object.value=nb_False;
          nbCellPublish(trick->trueCell);
          }
        trick->trueCell=trueCell;
       }
      } 
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
static void enableTrickRelEq(NB_TrickRelEq *trick){   
  //outMsg(0,'T',"enableTrickRelEq: called");
  }

static void disableTrickRelEq(NB_TrickRelEq *trick){
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

/**********************************************************************
* Public Methods
**********************************************************************/
void initTrick(NB_Stem *stem){
  //outMsg(0,'T',"initTrick: called");
  trickH=newHash(100003);  /* initialize condition hash */
  nb_TypeTrickRelEq=newType(stem,"AxonRelEq",NULL,0,printTrick,destroyTrickRelEq);
  nbCellType(nb_TypeTrickRelEq,solveTrick,evalTrickRelEq,enableTrickRelEq,disableTrickRelEq);
  //outMsg(0,'T',"initTrick: returning");
  }

void nbTrickRelEqEnable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  //outMsg(0,'T',"nbTrickRelEqEnable: called");
  //outPut("pub:");
  //printObject((NB_Object *)pub);
  //outPut("\nsub:");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  trick=useTrickRelEq(nb_TypeTrickRelEq,(NB_Cell *)cond->left);
  //outMsg(0,'T',"nbTrickRelEqEnable: 1");
  if(trick->cell.sub==NULL){  // subscribe to pub if we haven't already - have no subscribers
    trick->cell.level=pub->level+1;
    nbCellEnable(pub,(NB_Cell *)trick); 
    }
  //outMsg(0,'T',"nbTrickRelEqEnable: 2 trick->level=%d",trick->cell.level);
  nbCellEnableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelEq,NULL);
  cond->cell.object.value=cond->cell.object.type->eval(cond);  // evaluate when enabling
  if(cond->cell.object.value==NB_OBJECT_TRUE) trick->trueCell=(NB_Cell *)cond;
  if(pub->object.value==nb_Unknown) trick->trueCell=(NB_Cell *)nb_Unknown;
  
  // the next line will change when we are ready
  //nbCellEnable(pub,(NB_Cell *)cond);
  //outMsg(0,'T',"nbTrickRelEqEnable: returning");
  }

void nbTrickRelEqDisable(nbCELL pub,struct COND *cond){
  NB_TrickRelEq *trick;

  //outMsg(0,'T',"nbTrickRelEqDisable: called");
  //outPut("pub:");
  //printObject((NB_Object *)pub);
  //outPut("\nsub:");
  //printObject((NB_Object *)cond);
  //outPut("\n");
  //outFlush();
  trick=useTrickRelEq(nb_TypeTrickRelEq,(NB_Cell *)cond->left);
  if(trick->trueCell=(NB_Cell *)cond) trick->trueCell=NULL;
  nbCellDisableTrick((NB_Cell *)trick,(NB_Cell *)cond,compareTrickRelEq,NULL);
  if(trick->cell.sub==NULL){ // if we now have no subscribers
    nbCellDisable(pub,(NB_Cell *)trick);
    destroyTrickRelEq(trick);
    }
  //outMsg(0,'T',"nbTrickRelEqDisable: returning");
  }

