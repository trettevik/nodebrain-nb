/*
* Copyright (C) 1998-2009 The Boeing Company
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
* File:     nbstring.h 
*
* Title:    String Object Methods
*
* Function:
*
*   This file provides methods for nodebrain STRING objects that are defined
*   by a null terminated array of characters.  The STRING type extends the
*   OBJECT type defined in nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initString();
*   struct STRING *useString(char *string);
*   void printString();
*   void printStringRaw();
*   void printStringAll();
* 
* Description
*
*   String objects are constants to be shared by many objects in nodebrain
*   memory.  Obtain a pointer to a string objects by calling useString().  A   
*   new object is created if necessary.   
*
*     struct STRING *mystr=useString(char *string);
*
*   When assigning a string object pointer to another data structure, reserve
*   the string object by calling grabObject().  This increments a reference
*   count in the string object.
*
*     mystruct.strPtr=grabObject(useString("foo"));
*
*   When a reference to a string object is no longer needed, release the
*   reservation by calling dropObject().
*
*     dropObject(struct STRING *mystr);
*
*   When all references to a string are dropped, the memory assigned to the
*   object may be reused.
*  
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/06/28 Ed Trettevik (original prototype version introduced in 0.2.8)
*             1) This code is a revision to code previously imbedded
*                in nodebrain.c. The primary difference is the use of nbobj.h
*                routines.
* 2001/07/09 eat - version 0.2.8
*             1) Removed reference count increment from useString() function.
*             2) Included "next" pointer management in useString() and
*                destroyString().  This replaces logic removed from newObject()
*                and dropObject().
*             3) destroyString() now does the free(string) instead of
*                dropObject().
* 2002/05/06 eat - version 0.3.2
*             1) Fixed defect in logic to truncate leading zeroes on number.
* 2005/05/08 eat 0.6.2  change hashStr() to use int instead of long
*            This was done to ensure that string hash the same on 64 bit
*            machines as on 32 bit machines.  This only comes into play when
*            running check scripts that display term glossaries.  If they
*            don't hash the same, the order in which the terms display is
*            different.  Using int avoids that problem.
* 2008-01-22 eat 0.6.9  Modified string allocation scheme
*=============================================================================
*/
#include "nbi.h"

struct HASH *strH;

struct TYPE *strType;

struct NB_STRING_POOL{
  struct STRING *vector[NB_OBJECT_MANAGED_SIZE/8];
  };

struct NB_STRING_POOL *nb_StringPool;  // free string pool vector by length

void *hashStr(struct HASH *hash,char *cursor){
  /*
  *  Hash a string and return a pointer to a pointer in the hashing vector.
  *
  *  Note: The hashing algorithm can be improved for long strings
  *
  */
  unsigned int h=0;
  while(*cursor){
    h=((h<<3)+*cursor);
    cursor++;
    }
  return(&(hash->vect[h%hash->modulo]));
  }


/**********************************************************************
* Object Management Methods
**********************************************************************/
void printStringRaw(struct STRING *string){
  if(string==NULL) outPut("???");
  else outPut("%s",string->value);
  }
 
void printString(string) struct STRING *string;{
  outPut("\"%s\"",string->value);
  }

void printStringAll(){
  struct STRING *string,**stringP;
  long v;
  int i,saveshowcount=showcount;
  
  showcount=1;
  outPut("String Table:\n");
  stringP=(struct STRING **)&(strType->hash->vect);
  for(v=0;v<strType->hash->modulo;v++){
    i=0;
    for(string=*stringP;string!=NULL;string=(struct STRING *)string->object.next){
      outPut("[%u,%d]",v,i);
      printObject(string);
      outPut("\n");
      i++;
      }
    stringP++;  
    }
  showcount=saveshowcount;  
  }

void destroyString(str) struct STRING *str;{
  struct STRING *string,**stringP,**freeStringP;
  char *cursor,*value;
  int r=1;  /* temp relation <0, 0, >0 */
  int size;
  unsigned long h=0;
  unsigned char c;

  value=str->value;
  for(cursor=value;*cursor!=0;cursor++){
    c=*cursor;
    h=((h<<3)+c);
    }  
  stringP=(struct STRING **)&(strType->hash->vect[h%strType->hash->modulo]);
  for(string=*stringP;string!=NULL && (r=strcmp(string->value,value))>0;string=*stringP)
    stringP=(struct STRING **)&(string->object.next);
  if(string==NULL || r!=0){
    outMsg(0,'L',"destroyString() unable to locate string object.");
    return;
    }
  *stringP=(struct STRING *)string->object.next;    // remove from hash list
  size=sizeof(struct STRING)+strlen(str->value)+1;
  if(size>NB_OBJECT_MANAGED_SIZE) free(str);        // free large unmanaged strings
  else{
    freeStringP=&nb_StringPool->vector[(size+7)/8];         // keep managed strings in a pool by length
    string->object.next=(void *)*freeStringP; 
    *freeStringP=string;
    }
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void initString(NB_Stem *stem){
  nb_StringPool=(struct NB_STRING_POOL *)malloc(sizeof(struct NB_STRING_POOL));
  memset(nb_StringPool,0,sizeof(struct NB_STRING_POOL));
  strH=newHash(100003);
  strType=newType(stem,"string",strH,0,printString,destroyString);
  strType->apicelltype=NB_TYPE_STRING;
  }

struct STRING *useString(char *value){
  struct STRING *string,**stringP,**freeStringP;
  int size;
  char *cursor;
  int r=1;  /* temp relation <0, 0, >0 */
  unsigned long h=0;
  unsigned char c;
  
  // NOTE: need to change hashing algorithm to just use last x bytes
  for(cursor=value;*cursor!=0;cursor++){
    c=*cursor;
    h=((h<<3)+c);
    }  
  stringP=(struct STRING **)&(strType->hash->vect[h%strType->hash->modulo]);
  for(string=*stringP;string!=NULL && (r=strcmp(string->value,value))>0;string=*stringP)
    stringP=(struct STRING **)&(string->object.next);
  if(string!=NULL && r==0) return(string);

  size=sizeof(struct STRING)+strlen(value)+1;
  if(size>NB_OBJECT_MANAGED_SIZE) freeStringP=NULL;
  else freeStringP=&nb_StringPool->vector[(size+7)/8]; 
  string=(struct STRING *)newObject(strType,freeStringP,sizeof(struct STRING)+size);
  string->object.next=(NB_Object *)*stringP;     
  *stringP=string;  
  strcpy(string->value,value);
  return(string);
  }
