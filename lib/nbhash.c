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
* File:     nbhash.c 
* 
* Title:    Hash Object Routines
*
* Function:
*
*   This file provides routines that manage nodebrain hash objects.
*
*   These routines are called by the following object methods. 
*
*      1) Init    - creates hash to lookup identical instances
*      2) Use/New - lookup and insert new object in hash
*      3) Destroy - remove object from hash 
*
* Synopsis:
*
*   #include "nbhash.h"
*
*   void *nbHashInit();
*   NB_Hash *nbHashNew(int modulo);
*   void destroyHash(struct HASH *hash);
*
* Description
*
*   nbHashInit() is called once to define the hash object type.
*
*   nbHashNew(modulo) is called to allocate a hash with modulo entries
*
*   destroyHash(hash) is called when a hash is no longer needed.  This
*   function is currently a stub that does nothing.  It should destroy
*   every object in the hash and free the hash table.
*
*=============================================================================
* Enhancements:
*
*   o  Consider changing the object reference count to the places where the
*      reference assignments are made.  A function could be provided for this.
*
*            objectRef(&pointer,object);   dec and inc
*            pointer=objectRef(object);    inc 
*
*      When a new object is created with newObject or nbCellNew it should be
*      placed in a list of new objects.  At the end of each command cycle we
*      need to scan the list for unreferenced objects.
*
*      Think this through better before making this change.  It may not be an
*      improvement.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 Ed Trettevik (split out in 0.4.1)
* 2010-02-28 eat 0.4.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-31 eat 0.8.13 Checker updates
* 2014-01-12 eat 0.9.00 Included nbHashGrow function
* 2014-01-25 eat 0.9.00 Switch from storing modulo to storing mask (modulo-1)
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>
#include <stddef.h>

struct TYPE *typeHash;

/*
*  Display
*/
void nbHashStats(void){
  NB_Type *type;
  NB_Hash *hash;
  NB_Object *object,**objectP;
  long totalSlots=0;
  long totalSlotsUsed=0;
  long totalObjects=0;
  long otherSlots=0;
  long totalMaxList=0;
  int v,n,slotsUsed,maxList;
  double aveList;

  outPut("\n\nHash Statistics:\n\n");
  outPut("%20s %10s %10s %10s %10s %10s\n\n","Type","Slots","Used","Objects","MaxList","AveList");
  for(type=nb_TypeList;type!=NULL;type=(NB_Type *)type->object.next){
    hash=type->hash;
    if(hash->objects>0){
      slotsUsed=0;
      maxList=0;
      objectP=(NB_Object **)&(hash->vect);
      for(v=0;v<=hash->mask;v++){
        if(*objectP){
          slotsUsed++;
          for(n=0,object=*objectP;object!=NULL;n++,object=object->next);
          if(n>maxList) maxList=n;
          } 
        objectP++;
        }
      if(slotsUsed) aveList=(double)hash->objects/slotsUsed;
      else aveList=0;
      outPut("%20s %10u %10u %10u %10d %8.2f\n",type->name,hash->mask+1,slotsUsed,hash->objects,maxList,aveList);
      totalSlots+=hash->mask+1;
      totalSlotsUsed+=slotsUsed;
      if(maxList>totalMaxList) totalMaxList=maxList;
      totalObjects+=hash->objects;
      }
    else otherSlots+=hash->mask+1;
    }
  outPut("\n%20s %10ld %10ld %10ld %10ld %8.2f\n","Other",otherSlots,0,0,0,0.0);
  totalSlots+=otherSlots;
  if(totalSlotsUsed) aveList=(double)totalObjects/totalSlotsUsed;
  else aveList=0;
  outPut("\n%20s %10ld %10ld %10ld %10ld %8.2f\n\n","Total",totalSlots,totalSlotsUsed,totalObjects,totalMaxList,aveList);
  }
/* 
*  Hash constructor 
*
*  NOTE: The module must be a power of 2, we depend on the caller to enforce this
*/
NB_Hash *nbHashNew(size_t modulo){  // 2012-12-31 eat - VID 4547 - long to size_t, vectsize int to long
  NB_Hash *hash;
  size_t vectsize;

  if(modulo==0) modulo=8;
  else if(modulo>UINT32_MAX) nbExit("nbHashNew: Logic error - modulo %ld exceeds limit of %u - terminating",modulo,UINT32_MAX);
  vectsize=modulo*sizeof(void *);
  hash=nbAlloc(offsetof(struct HASH,vect)+vectsize);
  hash->object.next=NULL;
  hash->object.type=typeHash;
  hash->object.refcnt=0;
  hash->mask=modulo-1;
  hash->objects=0;
  hash->limit=modulo*.75;  // Grow when the hash is 75 percent full - this may be ++option
  memset(hash->vect,0,vectsize);
  return(hash);  
  }

void nbHashGrow(struct HASH **hashP){
  struct HASH *hash;
  struct NB_OBJECT *object,*objectNext,**objectP,**objectPa,**objectPb;
  int v;
  size_t vectsize;
  uint32_t modulo=(*hashP)->mask+1;
  size_t size=modulo*2;
  uint32_t mask=size-1;

  if(size>UINT32_MAX) nbExit("nbHashGrow: Logic error - modulo %ld exceeds limit of %u - terminating",size,UINT32_MAX);
  hash=nbHashNew(size);
  hash->objects=(*hashP)->objects;
  objectP=(NB_Object **)&((*hashP)->vect);
  for(v=0;v<=(*hashP)->mask;v++){
    if(*objectP){
      objectPa=(NB_Object **)&(hash->vect[v]);
      objectPb=(NB_Object **)&(hash->vect[v+modulo]);
      for(object=*objectP;object!=NULL;object=objectNext){
        objectNext=object->next;
        if((object->hashcode&mask)==v){
          *objectPa=object;
          objectPa=&object->next;
          }
        else{
          *objectPb=object;
          objectPb=&object->next;
          }
        }
      *objectPa=NULL;
      *objectPb=NULL;
      }
    objectP++;
    }
  vectsize=modulo*sizeof(void *);
  nbFree(*hashP,offsetof(struct HASH,vect)+vectsize); 
  *hashP=hash;
  }

/*
*  Print all or selected objects in a hash
*/
void nbHashShow(struct HASH *hash,char *label,NB_Type *type){
  NB_Object *object,**objectP;
  long v;
  int i;
  int32_t level;

  outPut("Hash: Modulo=%8.8x Objects=%8.8x %s\n",hash->mask+1,hash->objects,label);
  objectP=(NB_Object **)&(hash->vect);
  for(v=0;v<=hash->mask;v++){
    i=0;
    for(object=*objectP;object!=NULL;object=object->next){
      if(type==NULL || object->type==type){
        if(object==object->value) level=0;
        else level=((NB_Cell *)object)->level;
        outPut("Slot=%8.8x.%2.2x Code=%8.8x Ref=%8.8x Level=%8.8x = ",v,i,object->hashcode,object->refcnt,level);
        printObject(object->value);
        if(object!=object->value){
          outPut(" == "); 
          printObject(object);
          }
        outPut("\n");
        if(i<INT_MAX) i++;
        }
      }
    objectP++;
    }
  }

/*
*  Hash destructor
*/
static void destroyHash(NB_Object object){
  /* call dropObject for every object in the hash */
  }
 
/*
*  Initialize types provided by this header
*/
void nbHashInit(NB_Stem *stem){
  typeHash=nbObjectType(stem,"hash",0,0,nbHashShow,destroyHash);
  }
