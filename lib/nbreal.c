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
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *realType;
struct REAL *realFree=NULL;

// we need two version of this depending on the endian of the architecture
#define NB_HASH_REAL(HASHCODE,DOUBLE){ \
  double fraction; \
  HASHCODE=DOUBLE; \
  fraction=DOUBLE-HASHCODE; \
  fraction*=0x100000000; \
  HASHCODE^=(uint32_t)fraction; \
  }

/*
*  Hash a real and return a pointer to a pointer in the hash table.
*/
void *hashReal(NB_Hash *hash,double n){
  uint32_t hashcode;
  NB_HASH_REAL(hashcode,n)
  return(&(hash->vect[hashcode&hash->mask]));
  }

/*
void *hashReal(NB_Hash *hash,double n){
  unsigned long h,*l;
  l=(unsigned long *)&n;
  h=*l;
  return(&(hash->vect[h&hash->mask]));
  }
*/


NB_Real **locateReal(double n){
  NB_Real **realP;
 
  realP=hashReal(realType->hash,n);
  for(;*realP!=NULL && (*realP)->value<n;realP=(struct REAL **)&((*realP)->object.next));
  return(realP);
  }

/**********************************************************************
* Object Management Methods
**********************************************************************/
size_t realName(NB_Cell *context,NB_Real *real,char **nameP,size_t size){
  char number[20];
  size_t len;

  sprintf(number,"%.10g",real->value);
  len=strlen(number);
  if(size>len) strcpy(*nameP,number),(*nameP)+=len;
  size-=len;
  return(size);
  }

void printReal(NB_Real *real){
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
  realType=NbObjectType(stem,"real",NB_OBJECT_KIND_REAL|NB_OBJECT_KIND_CONSTANT|NB_OBJECT_KIND_TRUE,0,realName,printReal,destroyReal);
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
  uint32_t hashcode;
  struct HASH *hash=realType->hash;

  NB_HASH_REAL(hashcode,value)
  //outMsg(0,'T',"useReal: hashcode=%8.8x value=%f\n",hashcode,value);
  realP=(NB_Real **)&(hash->vect[hashcode&hash->mask]);
  for(;*realP!=NULL && (*realP)->value<value;realP=(struct REAL **)&((*realP)->object.next));
  if(*realP!=NULL && (*realP)->value==value) return(*realP);
  real=(struct REAL *)newObject(realType,(void **)&realFree,sizeof(struct REAL));
  real->object.hashcode=hashcode;
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

