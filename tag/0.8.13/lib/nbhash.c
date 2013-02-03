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
*   void *initHash();
*   struct HASH *newHash(int modulo);
*   void destroyHash(struct HASH *hash);
*
* Description
*
*   initHash() is called once to define the hash object type.
*
*   newHash(modulo) is called to allocate a hash with modulo entries
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
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *typeHash;
/* 
*  Hash constructor 
*/
struct HASH *newHash(size_t modulo){  // 2012-12-31 eat - VID 4547 - long to size_t, vectsize int to long
  /*
  *  Create a new hashing vector
  */
  struct HASH *hash;
  long vectsize;
  if(modulo>LONG_MAX/sizeof(void *)) nbExit("newHash: Logic error - modulo exceeds limit of %ld - terminating",LONG_MAX/sizeof(void *));
  vectsize=modulo*sizeof(void *);
  hash=nbAlloc(sizeof(struct HASH)+vectsize);
  hash->object.next=NULL;
  hash->object.type=typeHash;
  hash->object.refcnt=0;
  hash->modulo=modulo;
  memset(hash->vect,0,vectsize);
  return(hash);  
  }

/*
*  Print all or selected objects in a hash
*/
void printHash(struct HASH *hash,char *label,NB_Type *type){
  NB_Object *object,**objectP;
  long v;
  int i,saveshowcount=showcount;

  showcount=1;
  outPut("%s:\n",label);
  objectP=(NB_Object **)&(hash->vect);
  for(v=0;v<hash->modulo;v++){
    i=0;
    for(object=*objectP;object!=NULL;object=object->next){
      if(type==NULL || object->type==type){
        outPut("[%u,%d] = ",v,i);
        printObject(object->value);
        if(object!=object->value){
          outPut(" == "); 
          printObject(object);
          }
        outPut("\n");
        i++;
        }
      }
    objectP++;
    }
  showcount=saveshowcount;
  }

/*
*  Hash destructor
*/
void destroyHash(NB_Object object){
  /* call dropObject for every object in the hash */
  }
 
/*
*  Initialize types provided by this header
*/
void initHash(NB_Stem *stem){
  typeHash=newType(stem,"hash",NULL,0,printHash,destroyHash);
  }
