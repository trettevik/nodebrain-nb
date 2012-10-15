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
* File:     nbobject.c 
* 
* Title:    Object Management Routines (prototype)
*
* Function:
*
*   This header provides routines that manage nodebrain objects, keeping track
*   of object references so unreferenced objects can be deleted.  They also
*   support the change registration feature that enables nodebrain to react to
*   changing conditions efficiently.
*
*   These routines are called by the following methods associated with each 
*   object type.
*
*      1) Init    - creates type using newType
*      2) Use/New - create new object using newObject or nbCellNew
*      3) Destroy - calls dropObject for all referenced objects
*
* Synopsis:
*
*   #include "nbobject.h"
*
*   void *newObject(struct TYPE *type,void **pool,short length);
*   void *grabObject(void *object);    
*   void dropObject(void *object);
*   void printObject(void *object);
*   void printObjectType(void *object);
* 
* Description
*
*   newObject    - Allocate an object
*   grabObject   - Increment reference counter
*   dropObject   - Decrement reference counter
*
*   For each type of object you use with this API, the following steps are
*   required.
*
*   1) Define a structure using an OBJECT header.
*
*        struct MYOBJ{
*          NB_Object object;
*          ...
*          };
*
*
*   2) Define a type structure;
*
*        struct TYPE myobjType;
*
*   3) Write an "init" method to perform initialization functions.  In
*      particular, this method must call newType to define the type.
*
*        myobjType=newType(stem,name,hash,attributes,myobjPrint,myobjDestroy);
*
*      The method arguments are routines you write as described later. 
*
*   4) Write a "locate" method.  This method will be called by other code
*      you write.  It should use the parameters passed to search for
*      the existance of a matching instance.  Return 1 if found and 0 if
*      not found.  The hash field of the TYPE structure and the next field
*      of the OBJECT structure are used along with the parameters to hash
*      search.  If found the object field of the TYPE structure is set to
*      the location of the object found.  If not found, the object field
*      of the TYPE structure is set to the instance where a new instance
*      should be inserted. 
*
*        myobject *myobjLocate(...){ ... }
*
*      This method is only required for objects you will need to be able
*      to locate.
*
*   5) Write a "use" method.  This method will be called by other
*      code you write.
*
*        myobject *useMyobj(...){ ... }
*
*      If you have a locate method, call it to see if an instance already
*      exists.  If so, just return the address returned by the locate method.
*      If you do not have a locate method, or the locate method returned zero,
*      you must call newObject or nbCellNew to allocate a new instance.
*
*        myobj=newObject(mytype,pool,length);
*
*      The constructor must initialize all fields after the object header.
*      (The header is initialized by newObject or nbCellNew.)  If your object
*      structure contains pointers to other objects, you must call grabObject() 
*      to increment the reference count.  You may also want to call funSub
*      to subscribe to changes to the referenced object.
* 
*   6) Write a "destroy" method.  This method is required to call
*      dropObject for every object it referenced with a grabObject.
*
*        void destroyMyobj(struct MYOBJ *myobj){ ... }
*
*      If you have called funSub to subscribe for changes to other
*      objects, you must call funCan in the destroy method.
*
*      The method is also responsible for "unlinking" an object from
*      any directory list or hash that does not hold a reservation.
*
*   7) Write a "print" method. This method should print a symbolic 
*      representation of your object.
*
*        void printMyobj(struct MYOBJ *myobj){ ... }
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
* 2001/06/26 Ed Trettevik (original prototype version introduced in 0.2.8)
*             1) This code is a generalization of code previously imbedded
*                in nodebrain.c
* 2001/07/04 eat - version 0.2.8
*             1) The VARIABLE object integer state have been replaced with
*                a more general pointer to object.
*             2) Proact and React methods for all types must now return OBJECT*.
*             3) The NB_Cell object replaces VARIABLE object.  A simple
*                variable is treated as a cell that returns the value
*                of the constant or cell assigned to it.
*             4) Terminology changes:  
*                [snip] (2003/11/03 removed because very obsolete.) 
* 2001/07/08 eat - version 0.2.8
*             1) Split NB_Cell routines out to nbcell.h
* 2001/07/09 eat - version 0.2.8
*             1) Removed "next" link management from newObject() and
*                dropObject().  
*             2) Removed free() from dropObject.  This is now the responsibility
*                of an objects destroy function, including removing the object
*                from hashing tables or other references that don't hold a
*                reservation on the object.
*             3) Created grabObject() as a replacement for objRef().
* 2002/08/20 eat - version 0.4.0
*             1) Included "alert" and "parse" methods in the TYPE structure.
*                The "alert" method allows objects other than functions to be
*                alerted by the timer queue (nbclock.c).  Initially
*                we'll use this to alert a cache, which is not currently defined
*                as a function, although that may change later.  But in general
*                it seems like a good idea to be able to alert any type of
*                object.  (Object alerting should not be confused with context
*                alerting via the "alert" command.)
*
* 2003/11/18 eat 0.5.5  Merged nbtype.c into nbobject.c
* 2008/01/22 eat 0.6.9  Improved management of object memory
* 2008-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-01-25 eat - included check for malloc failures in nbAlloc
* 2012-10-13 eat 0.8.12 Replaced other uses malloc with nbAlloc
* 2012-10-13 eat 0.8.12 Added nbHeap function (was first part of nbObjectInit)
*=============================================================================
*/
#include "nbi.h"

int showstate=0;         /* show state information in Show command */
int showvalue=0;         /* show function values */
int showlevel=0;         /* show function level  */
int showcount=0;         /* show object usage count */


NB_Object *nb_Disabled=NULL;
NB_Object *nb_Unknown=NULL;
NB_Object *nb_Undefined=NULL;
NB_Object *nb_Placeholder=NULL;

NB_Type *nb_DisabledType; 
NB_Type *nb_UnknownType;  
NB_Type *nb_UndefinedType;  
NB_Type *nb_PlaceholderType;  
NB_Type *nb_TypeList=NULL;
NB_Type *nb_TypeType;

/*
*  Primitive object heap 
*
*    We use a simple strategy that does not release system memory once aquired,
*    but does release individual objects for reuse within the same object pool.
*    Frequently pools are dedicated to objects of a specific type, but can be
*    shared by types having the same size.
*/

#define NB_OBJECT_PAGE_SIZE 128*1024

struct NB_OBJECT_PAGE{
  struct NB_OBJECT_PAGE *next;
  char *top;
  char space[NB_OBJECT_PAGE_SIZE-2*sizeof(void *)];
  };

struct NB_OBJECT_PAGE *objectHeap;

struct NB_OBJECT_POOL{
  struct STRING *vector[NB_OBJECT_MANAGED_SIZE/8];
  };

struct NB_OBJECT_POOL *nb_ObjectPool;  // free object pool vector by length


/*
*  Print methods for special objects used by cell routines.
*/
void nbDisabledShow(NB_Object *object){
  outPut("!");
  }
void nbUnknownShow(NB_Object *object){
  outPut("?");
  }
void nbUndefinedShow(NB_Object *object){
  outPut("??");
  }
void nbPlaceholderShow(NB_Object *object){
  outPut("_");
  }
void nbTypeShow(NB_Type *type){
  outPut("type %s",type->name);
  }

void *nbHeap(){
  objectHeap=malloc(NB_OBJECT_PAGE_SIZE);
  if(!objectHeap){
    fprintf(stderr,"NodeBrain out of memory.  Terminating\n");
    exit(NB_EXITCODE_FAIL);
    }
  objectHeap->next=NULL;
  objectHeap->top=(char *)objectHeap+NB_OBJECT_PAGE_SIZE;
  nb_ObjectPool=malloc(sizeof(struct NB_OBJECT_POOL));
  if(!objectHeap){
    fprintf(stderr,"NodeBrain out of memory.  Terminating\n");
    exit(NB_EXITCODE_FAIL);
    }
  memset(nb_ObjectPool,0,sizeof(struct NB_OBJECT_POOL));
  }

void nbObjectInit(NB_Stem *stem){
  nb_DisabledType=newType(stem,"disabled",NULL,TYPE_SPECIAL,nbDisabledShow,NULL);
  nb_DisabledType->apicelltype=NB_TYPE_DISABLED;
  nb_Disabled=newObject(nb_DisabledType,NULL,sizeof(NB_Object));
  nb_Disabled->refcnt=(unsigned int)-1;   /* flag as perminent object */

  nb_UnknownType=newType(stem,"unknown",NULL,TYPE_SPECIAL,nbUnknownShow,NULL);
  nb_UnknownType->apicelltype=NB_TYPE_UNKNOWN;
  nb_Unknown=newObject(nb_UnknownType,NULL,sizeof(NB_Object));
  nb_Unknown->refcnt=(unsigned int)-1;    /* flag as perminent object */

  nb_UndefinedType=newType(stem,"undefined",NULL,TYPE_SPECIAL,nbUndefinedShow,NULL);
  nb_UndefinedType->apicelltype=NB_TYPE_UNDEFINED;
  nb_Undefined=newObject(nb_UndefinedType,NULL,sizeof(NB_Object));
  nb_Undefined->refcnt=(unsigned int)-1;    /* flag as perminent object */

  nb_PlaceholderType=newType(stem,"placeholder",NULL,TYPE_SPECIAL,nbPlaceholderShow,NULL);
  nb_PlaceholderType->apicelltype=NB_TYPE_PLACEHOLDER;
  nb_Placeholder=newObject(nb_PlaceholderType,NULL,sizeof(NB_Object));
  nb_Placeholder->refcnt=(unsigned int)-1;    /* flag as perminent object */

  nb_DisabledType->object.value=nb_Unknown;
  nb_UnknownType->object.value=nb_Unknown;
  nb_UndefinedType->object.value=nb_Unknown;
  nb_PlaceholderType->object.value=nb_Unknown;

  nb_TypeType=newType(stem,"type",NULL,TYPE_WELDED,nbTypeShow,NULL);
  }
/*
*  Obtain space for a new constant object instance
*
*    This can be improved significantly.  For now we manage the space
*    used by objects not larger than NB_OBJECT_MANAGED_SIZE.
*    Currently, individual object types manage their free object lists.
*    In the future, we need to move that here, following the scheme
*    used currently by the STRING object, where free lists are maintained
*    for blocks by length, in increments of 8.  The pool parementer can
*    then be removed, and dropObject can return space to the appropriate
*    free list.
*/
void *newObject(struct TYPE *type,void **pool,int size){
  NB_Object *object,**freeItemP;
  if(size>NB_OBJECT_MANAGED_SIZE){
    object=malloc(size);
    if(!object){ 
      fprintf(stderr,"NodeBrain out of memory.  Terminating\n");
      exit(NB_EXITCODE_FAIL);
      }
    }
  else if(pool==NULL || (object=*pool)==NULL){
    size=(size+7)&-8;  // convert to index rounded to 8 byte units
    freeItemP=(NB_Object **)&nb_ObjectPool->vector[(size-1)>>3];  // index in 8 byte units
    if((object=*freeItemP)==NULL){
      if((objectHeap->top)-(char *)&objectHeap->space<size){
        struct NB_OBJECT_PAGE *newPage=nbAlloc(NB_OBJECT_PAGE_SIZE);
        newPage->next=objectHeap;
        objectHeap=newPage;
        objectHeap->top=(char *)objectHeap+NB_OBJECT_PAGE_SIZE;
        }
      objectHeap->top-=size;
      object=(NB_Object *)objectHeap->top;
      }
    else *freeItemP=object->next;
    }
  else *pool=object->next;
  object->type=type;
  object->value=object;
  object->refcnt=0;
  return(object);
  }

// 2012-01-25 eat - included check for malloc failures

void *nbAlloc(int size){
  NB_Object *object,**freeItemP;

  //outMsg(0,'T',"nbAlloc: size=%d",size);
  if(size>NB_OBJECT_MANAGED_SIZE){
    object=(NB_Object *)malloc(size);
    if(!object){ 
      fprintf(stderr,"NodeBrain out of memory.  Terminating\n");
      exit(NB_EXITCODE_FAIL);
      }
    //outMsg(0,'T',"nbAlloc: object=%p size=%d",object,size);
    return(object);
    }
  size=(size+7)&-8;  // convert to index rounded to 8 byte units
  freeItemP=(NB_Object **)&nb_ObjectPool->vector[(size-1)>>3];  // index in 8 byte units
  if((object=*freeItemP)==NULL){
    if((objectHeap->top)-(char *)&objectHeap->space<size){
      struct NB_OBJECT_PAGE *newPage=malloc(NB_OBJECT_PAGE_SIZE);
      if(!newPage){
        fprintf(stderr,"NodeBrain out of memory.  Terminating\n");
        exit(NB_EXITCODE_FAIL);
        }
      newPage->next=objectHeap;
      objectHeap=newPage;
      objectHeap->top=(char *)objectHeap+NB_OBJECT_PAGE_SIZE;
      }
    objectHeap->top-=size;
    object=(NB_Object *)objectHeap->top;
    }
  else *freeItemP=object->next;
  //outMsg(0,'T',"nbAlloc: object=%p size=%d",object,size);
  return(object); 
  }

void nbFree(void *object,int size){
  NB_Object **freeItemP;

  //outMsg(0,'T',"nbFree: object=%p size=%d",object,size);
  if(size>NB_OBJECT_MANAGED_SIZE){
    //outMsg(0,'T',"Freeing unmanaged size %d",size);
    free(object);
    return;
    }
  size=(size+7)&-8; // convert to index rounded to 8 byte units
  freeItemP=(NB_Object **)&nb_ObjectPool->vector[(size-1)>>3];
  ((NB_Object *)object)->next=*freeItemP;
  *freeItemP=(NB_Object *)object; 
  }

/*
*  Increment reference count
*/
void *grabObject(void *object){
  if(object==NULL){
    outMsg(0,'L',"grabObject() called with NULL pointer - deprecated support will be removed.");
    return(nb_Unknown);  /* makes life easier for the caller */
    }
  /* note: perminent object can be assigned internally without using grab/drop */
  if(((NB_Object *)object)->refcnt==(unsigned int)-1) return(object); /* perminent object */
  (((NB_Object *)object)->refcnt)++;
  return(object);
  }

/*
*  Increment reference count
*/
void *grabObjectNull(void *object){
  if(object==NULL) return(NULL); // make life easier for the caller          
  /* note: perminent object can be assigned internally without using grab/drop */
  if(((NB_Object *)object)->refcnt==(unsigned int)-1) return(object); /* perminent object */
  (((NB_Object *)object)->refcnt)++;
  return(object);
  }


/*
*  Decrement reference count and destroy object when zero
*
*    Note: we use a void pointer parameter to simplify calls using several
*    different types of object structures.
*/
void *dropObject(void *obj){
  NB_Object *object=obj;
  //if(trace) outMsg(0,'T',"dropObject called");
  if(object==NULL){
    //outMsg(0,'L',"dropObject() called with NULL pointer - deprecated support will be removed.");
    return(NULL);  /* makes life easier for the caller */
    }
  if(((NB_Object *)object)->refcnt==(unsigned int)-1) return(NULL); /* preserve perminent objects */
  if(object->refcnt>0) object->refcnt--;
  if(object->refcnt==0){
    if(object->value!=object) object->value=dropObject(object->value);
    object->type->destroy(object);
    }
  return(NULL);
  }

/*
*  Decrement reference count and destroy object when zero
*
*    Note: we use a void pointer parameter to simplify calls using several
*    different types of object structures. 
*/
void *dropObjectNull(void *obj){
  NB_Object *object=obj;
  if(object==NULL) return(NULL);  // make life easier for the caller
  if(((NB_Object *)object)->refcnt==(unsigned int)-1) return(NULL); /* preserve perminent objects */
  if(object->refcnt>0) object->refcnt--;
  if(object->refcnt==0){
    if(object->value!=object) object->value=dropObject(object->value);
    object->type->destroy(object);
    }
  return(NULL);
  }

/*
*  Object Print
*/
void printObject(NB_Object *object){
  if(object==NULL) outPut("???");
  else{
    if(object->value!=object){      /* function */
      if(showvalue || showstate){
        outPut("{");
        printObject(object->value);
        outPut("}");
        }
      if(showlevel) outPut("(%d)",((NB_Cell *)object)->level);
      }
    if(showcount){
      outPut("[%d]",object->refcnt);
      }
    object->type->showExpr(object);
    }
  }

void printObjectReport(NB_Object *object){
  if(object->type->showReport!=object->type->showItem) object->type->showReport(object);
  }

void printObjectItem(NB_Object *object){
  if(object->type==NULL) outMsg(0,'T',"object type is null");
  if(object->type->showItem==NULL) outMsg(0,'T',"object type showItem is null");
  object->type->showItem(object);
  }

void printObjectType(object) void *object;{
  outPut(((NB_Object *)object)->type->name);
  }
/*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbtype.c 
* 
* Title:    Object Type Management Routines
*
* Function:
*
*   This file provides routines that support nodebrain object types.
*
*   These routines are called by the following methods associated with each 
*   object type.
*
*      1) Init    - creates type using newType
*      2) Use/New - create new object using newObject or nbCellNew
*      3) Destroy - calls dropObject for all referenced objects
*
* Synopsis:
*
*   #include "nb.h"
*
*   struct TYPE *newType(
*      char *name,
*      struct HASH *hash,
*      int attributes,
*      void (*display)(),
*      void (*destroy)() );
*
*   void *newObject(struct TYPE *type,void **pool,short length);
*
*   void *grabObject(void *object);    
*   void dropObject(void *object);
*   void printObject(void *object);
* 
* Description
*
*   Before writing code for a new object type, see nbobject.c for a description
*   of the API
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/08/31 Ed Trettevik (split out in 0.4.1)
*=============================================================================
*/


/*
*  Default for print and destroy methods.
*/
void nullVoid(object) NB_Object *object; {
  }

/*
*  Some handy default object methods to make sure we know what we're doing
*/
void printBug(object) NB_Object *object;{
  outPut("Object: ");
  printObject(object);
  outPut("\n");
  }

void alertBug(object) NB_Object *object; {
  outMsg(0,'L',"alertBug(): Object has no alert method.");
  printBug(object);
  }

void alarmBug(object) NB_Object *object; {
  outMsg(0,'L',"alertBug(): Object has no alarm method.");
  printBug(object);
  }

/* This is a future thing. */
NB_Object *parseBug(object) NB_Object *object; {
  outMsg(0,'L',"parseBug(): Object has no parse method.");
  printBug(object);
  return(object); 
  }

/*
*  Some handy default function methods to make sure we know what we're doing
*/
void solveBug(NB_Object *object){
  outMsg(0,'L',"solveBug(): Function has no solve method");
  printBug(object);
  return;
  }

NB_Object *evalStub(NB_Object *object){
  return(object);
  }
double evalDoubleBug(double x,double y){
  outMsg(0,'L',"evalDoubleBug(): Function has no method");
  return(0);
  }
char *evalStringBug(char *x,char *y){
  outMsg(0,'L',"evalStringBug(): Function has no method");
  return("");
  }
void enableBug(NB_Object *object){
  outMsg(0,'L',"enableBug(): Function has no enable method");
  printBug(object);
  }
void disableBug(NB_Object *object){
  outMsg(0,'L',"disableBug(): Function has no disable method");
  printBug(object);
  }

/*
*  Type Constructor
*/
struct TYPE *newType(NB_Stem *stem,char *name,struct HASH *hash,int  attributes,void (*showExpr)(),void (*destroy)()){
  struct TYPE *type;
  type=nbAlloc(sizeof(struct TYPE));
  type->object.type=nb_TypeType;
  type->object.next=(NB_Object *)nb_TypeList;
  nb_TypeList=type;
  type->object.value=nb_Unknown;
  type->object.refcnt=1;
  type->stem=stem;           
  type->name=name;
  type->hash=hash;
  type->attributes=attributes;
  type->apicelltype=0;
  if(showExpr==NULL) type->showExpr=&nullVoid;
  else type->showExpr=showExpr;
  type->showItem=type->showExpr;
  type->showReport=type->showExpr;
  if(destroy==NULL) type->destroy=&nullVoid;
  else type->destroy=destroy;
  type->alert=&alertBug;
  type->alarm=&alarmBug;  /* clock alarms */
  type->parse=&parseBug;
  type->construct=&parseBug;
  type->solve=&solveBug;
/* 2004/08/28 eat - experimenting with compute function */
  type->compute=&nbCellCompute_;
  type->eval=&evalStub;
  type->evalDouble=&evalDoubleBug;
  type->evalString=&evalStringBug;
  type->enable=&enableBug;
  type->disable=&disableBug;
  type->shim=NULL;
  return(type);
  }

// Show Types

void nbObjectShowTypes(void){
  struct TYPE *type;
  struct NB_TYPE_SHIM *shim;
  outPut("Type\tAlarm\tAlert\tEval\n");
  for(type=nb_TypeList;type!=NULL;type=(struct TYPE *)type->object.next){
    if((shim=type->shim)!=NULL) outPut("%s\t%8.3e %6.3e %5.3e\n",type->name,(double)type->shim->alarmTicks,(double)type->shim->alertTicks,(double)type->shim->evalTicks);
    else outPut("%s\n",type->name);
    }
  }
