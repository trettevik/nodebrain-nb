/*
* Copyright (C) 1998-2013 The Boeing Company
* Copyright (C) 2014      Ed Trettevik <eat@nodebrain.org>
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
* File:     nbstring.c 
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
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-02-07 dtl Checker updates
* 2012-10-12 eat 0.8.12 Replaced malloc with nbAlloc
* 2013-01-01 eat 0.8.13 Checker updates
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *strType;

struct NB_STRING_POOL{
  struct STRING *vector[NB_OBJECT_MANAGED_SIZE/8];
  };

struct NB_STRING_POOL *nb_StringPool;  // free string pool vector by length

/*
*  Hash a string and return a pointer to a pointer in the hash vector.
*
*/
void *hashStr(struct HASH *hash,char *cursor){
  uint32_t h=0;
  while(*cursor){
    h=((h<<3)+*cursor);
    cursor++;
    }
  return(&(hash->vect[h&hash->mask]));
  }

/**********************************************************************
* Object Management Methods
**********************************************************************/
void printStringRaw(struct STRING *string){
  if(string==NULL) outPut("???");
  else outPut("%s",string->value);
  }
 
void printString(struct STRING *string){
  outPut("\"%s\"",string->value);
  }

void printStringAll(void){
  nbHashShow(strType->hash,"Strings",NULL);
  }

void destroyString(NB_String *str){
  struct STRING *string,**stringP,**freeStringP;
  int r=1;  /* temp relation <0, 0, >0 */
  int size;
  NB_Hash *hash=strType->hash;

  //outMsg(0,'T',"destroyString: called for %s refcnt=%d",str->value,str->object.refcnt);
  stringP=(NB_String **)&(hash->vect[str->object.hashcode&hash->mask]);
  for(string=*stringP;string!=NULL && (r=strcmp(str->value,string->value))>0;string=*stringP)
    stringP=(NB_String **)&(string->object.next);
  if(string==NULL || r!=0){
    outMsg(0,'L',"destroyString: unable to locate string object.");
    outMsg(0,'L',"destroyString: hashcode=%u modulo=%u value='%s'",str->object.hashcode,hash->mask+1,str->value);
    return;
    }
  *stringP=(struct STRING *)string->object.next;    // remove from hash list
  size=sizeof(struct STRING)+strlen(str->value);
  size=(size+7)&-8;  // 2013-01-04
  //if(size>NB_OBJECT_MANAGED_SIZE) free(str);       // 2012-12-15 eat - CID 751590
  if(size>=NB_OBJECT_MANAGED_SIZE) free(str);        // free large unmanaged strings
  else{
    freeStringP=&nb_StringPool->vector[(size-1)>>3]; // keep managed strings in a pool by length  // 2013-01-04 eat - changed index calc
    string->object.next=(void *)*freeStringP; 
    *freeStringP=string;
    }
  hash->objects--;
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void initString(NB_Stem *stem){
  nb_StringPool=(struct NB_STRING_POOL *)nbAlloc(sizeof(struct NB_STRING_POOL));
  memset(nb_StringPool,0,sizeof(struct NB_STRING_POOL));
  strType=nbObjectType(stem,"string",NB_OBJECT_KIND_STRING|NB_OBJECT_KIND_CONSTANT|NB_OBJECT_KIND_TRUE,0,printString,destroyString);
  strType->apicelltype=NB_TYPE_STRING;
  }

struct STRING *useString(char *value){
  struct STRING *string,**stringP,**freeStringP;
  size_t size,len;
  int r=1;  /* temp relation <0, 0, >0 */
  NB_Hash *hash=strType->hash;
  uint32_t hashcode=0;
  
  NB_HASH_STR(hashcode,value)
  //outMsg(0,'T',"useString: hashcode=%8.8x value=%s\n",hashcode,value);
  stringP=(NB_String **)&(hash->vect[hashcode&hash->mask]);
  for(string=*stringP;string!=NULL && (r=strcmp(value,string->value))>0;string=*stringP)
    stringP=(struct STRING **)&(string->object.next);
  if(string!=NULL && r==0) return(string);
  len=strlen(value);
  size=sizeof(struct STRING)+len;
  size=(size+7)&-8;   // 2013-01-04 eat 
  if(size>=NB_OBJECT_MANAGED_SIZE) freeStringP=NULL;
  else freeStringP=&nb_StringPool->vector[(size-1)>>3]; // 2013-01-04 eat - changed index calc
  string=(struct STRING *)newObject(strType,(void **)freeStringP,size);
  len++; // 2013-01-14 eat - this is completely unnecessary, but replaced strcpy with strncpy to see if the checker is ok with that.
  strncpy((char *)string->value,value,len);  // 2013-01-01 eat - VID 5538-0.8.13-01 FP - we allocated enough space with call to newObject
  string->object.hashcode=hashcode;
  string->object.next=(NB_Object *)*stringP;     
  *stringP=string;  
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&strType->hash);
  return(string);
  }
