/*
* Copyright (C) 1998-2014 The Boeing Company
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
* File:     nbreal.c 
*
* Title:    Real Number Object Methods
*
* Function:
*
*   This file provides methods for nodebrain REAL objects, implemented as
*   DOUBLE in C.  The REAL type extends the OBJECT type defined in nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbRealInit();
*   struct STRING *useReal(char *string);
*   void printReal();
*   void printRealAll();
* 
* Description
*
*   Real objects may be constants or variables.  Constants may be shared
*   by many objects in nodebrain memory.  Obtain a pointer to a constant
*   real object by calling useReal().  A new object is created if necessary.
*
*     struct REAL *myreal=useReal(char *string);
*
*   When assigning a constant real object pointer to another data structure,
*   reserve the real object by calling grabObject().  This increments a reference
*   count in the real object.
*
*     mystruct.realPtr=grabObject(useReal("10.987"));
*
*   When a reference to a constant real object is no longer needed, release
*   the reservation by calling dropObject().
*
*     dropObject(struct REAL *myreal);
*
*   When all references to a constant real are dropped, the memory assigned to
*   the object may be reused.
*
*   Variable real objects are referenced by real functions, including terms
*   that represent variable real numbers.  To obtain a variable real object
*   call newReal();
*
*     struct REAL *myreal=newReal(double real);
*
*   For consistency, we call grabObject() and dropObject() for variables also,
*   but normally there is only one reference to a variable real.
*  
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 eat 0.4.1  Introduced prototype
* 2003-03-15 eat 0.5.1  Modified to conform to make file
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *realType;
struct REAL *realFree=NULL;

/*
*  Hash a real and return a pointer to a pointer in the hash table.
*/
void *hashReal(struct HASH *hash,double n){
  unsigned long h,*l;
  l=(unsigned long *)&n;
  h=*l;
  return(&(hash->vect[h&hash->mask]));
  }

struct REAL **locateReal(n) double n; {
  struct REAL **realP;
 
  realP=hashReal(realType->hash,n);
  for(;*realP!=NULL && (*realP)->value<n;realP=(struct REAL **)&((*realP)->object.next));
  return(realP);
  }

/**********************************************************************
* Object Management Methods
**********************************************************************/
void printReal(struct REAL *real){
  if(real==NULL) outPut("???");
  else outPut("%.10g",real->value);
  }


/*
*  Print all real constants
*/
void printRealAll(void){
  nbHashShow(realType->hash,"Numbers",NULL);
  }

void destroyReal(struct REAL *real){
  struct HASH *hash=realType->hash;
  struct REAL **realP;

  realP=(NB_Real **)&(hash->vect[real->object.hashcode&hash->mask]);
  for(;*realP!=NULL && *realP!=real;realP=(struct REAL **)&((*realP)->object.next));
  if(*realP!=real){
    outMsg(0,'L',"destroyReal: unable to locate real object.");
    outPut("value: ");
    printReal(real);
    outPut("\n");
    outFlush();
    return;
    }
  *realP=(struct REAL *)real->object.next;
  real->object.next=(NB_Object *)realFree;
  realFree=real;
  hash->objects--;
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbRealInit(NB_Stem *stem){
  realType=newType(stem,"real",NULL,0,printReal,destroyReal);
  realType->apicelltype=NB_TYPE_REAL;
  }

struct REAL *newReal(double value){
  struct REAL *real;

  real=(struct REAL *)newObject(realType,(void **)&realFree,sizeof(struct REAL));
  real->object.next=NULL;
  real->value=value;
  return(real);
  }

struct REAL *useReal(double value){
  NB_Real *real,**realP;
  unsigned long h;
  struct HASH *hash=realType->hash;

  h=value;  // change this so all fractions don't collide
  realP=(NB_Real **)&(hash->vect[h&hash->mask]);
  for(;*realP!=NULL && (*realP)->value<value;realP=(struct REAL **)&((*realP)->object.next));
  if(*realP!=NULL && (*realP)->value==value) return(*realP);
  real=(struct REAL *)newObject(realType,(void **)&realFree,sizeof(struct REAL));
  real->object.hashcode=h;
  real->value=value;
  real->object.next=(NB_Object *)*realP;
  *realP=real;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&realType->hash);
  return(real);
  }

struct REAL *parseReal(char *source){
  double value;
  char *cursor;

  if(trace) outMsg(0,'T',"parseReal() called - %s",source);
  value=strtod(source,&cursor);
  if(*cursor!=0){
    outMsg(0,'L',"useReal(): source does not conform to real syntax");
    return(NULL);
    }
  return(useReal(value));
  }

