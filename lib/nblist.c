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
* File:     nblist.c 
* 
* Title:    Object List Management Routines (prototype)
*
* Function:
*
*   This file provides routines that manage nodebrain object lists.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void listInsert(NB_Link **memberP,NB_Object *object);
*   void listRemove(NB_Link **memberP,NB_Object *object);   
*
*   NB_Link *parseList(NB_Term *context,char **cursor);
*   NB_Link *useList(NB_Link *link);
*   void dropList(NB_Link *list);
*
* Description
*
*   An object list is simply a linked list of pointers to objects.  Although
*   we may support a "list object" in the future, an object list is not
*   an object.  An object list is a lower level structure.
*
*   The listInsert() and listRemove() functions operate on unordered lists.
*
*   The parseList() function creates a list from a character string
*   representation.  List elements are separated by commas.  A list element
*   may be a string literal, a number (converts to string literal), or
*   a term.
*
*         "happy","days",fred,3
*
*   The dropList() function is used to decrement the usage counts of the
*   objects and return the list elements to the free list element list.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-25 Ed Trettevik (original prototype version introduced in 0.4.0)
*             1) Some of this code was split out from nbcell.h
* 2002-09-08 eat - version 0.4.1
*             1) Included code to set list function level.
* 2003-12-09 eat 0.6.0  Included API Functions
* 2004-08-28 eat 0.6.1  modified grabs associated with compute method
* 2005-05-01 eat 0.6.2  refined handling of empty lists
*            An empty list () must be recognized as different from a list
*            with an Unknown entry (??) or a list with a placeholder.  The
*            Unknown value does not make a good placeholder, so we are
*            creating a new placeholder cell type represented by a single
*            underscore. This cell is recognized as a True value in logical
*            expressions.  It is up to individual skill modules to determine
*            how to interpret it in argument lists.
*
*               mynode()      - empty list
*               mynode(_)     - list with one placeholder
*               mynode(a,_,b) - list with placeholder for second member 
*               mynode(a,,b)  - alternate syntax using implicit placeholder
*            
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2010-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-31 eat 0.8.13 Change hash size from int to size_t
* 2014-01-25 eat 0.9.00 Updated for changes to hash structure
*=============================================================================
*/
#include <nb/nbi.h>

NB_Link *nb_LinkFree=NULL; /* free link list */
NB_List *nb_ListFree=NULL; /* free list list */
NB_List *nb_ListNull=NULL; /* null list pointer */

struct TYPE   *nb_ListType;

void listInsert(NB_Link **memberP,void *object){
  /*
  *  Insert object in unordered list.
  */
  NB_Link *list;
  if((list=nb_LinkFree)==NULL) list=nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=list->next;
  list->next=*memberP;
  list->object=object;
  *memberP=list;
  }

/*
*  Insert object in unique ordered list.
*/
int listInsertUnique(NB_Link **memberP,void *object){
  NB_Link *list;
  for(;*memberP!=NULL && (*memberP)->object<(NB_Object *)object;memberP=&((*memberP)->next));
  if(*memberP!=NULL && (*memberP)->object==(NB_Object *)object) return(0);
  if((list=nb_LinkFree)==NULL) list=nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=list->next;
  list->next=*memberP;
  list->object=object;
  *memberP=list;
  return(1);
  }

/*
*  Free a list (without dropObject() calls).
*
*    The list may be the tail of a list, but caller must clean up the list head.
*/
void nbListFree(NB_Link *member){
  NB_Link *free=nb_LinkFree;
  
  if(member==NULL) return;
  nb_LinkFree=member;
  for(;member->next!=NULL;member=member->next);
  member->next=free;  
  } 
  
void listRemove(NB_Link **memberP,void *object){
  /* 
  *  Remove object from unordered list.
  */
  NB_Link *list;
  for(list=*memberP;list!=NULL && list->object!=object;list=*memberP) memberP=&(list->next);
  if(list!=NULL){  /* Remove list entry */
    *memberP=list->next;
    list->next=nb_LinkFree;
    nb_LinkFree=list;
    }
  else outMsg(0,'L',"listRemove - list entry not found.");    
  } 

/*
*  Compute cells in a list and return a list of values
*/
NB_List *nbListCompute(NB_List *cellList){
  NB_Link *head,*item,*entry,**next;
  NB_Object *object,*obj;

  next=&(head);
  for(item=cellList->link;item!=NULL;item=item->next){
    if((entry=nb_LinkFree)==NULL) entry=nbAlloc(sizeof(NB_Link));
    else nb_LinkFree=entry->next;
    *next=entry;
    entry->next=NULL;
    next=&(entry->next);
    object=item->object;
    object=object->type->compute(object);
    /*
       2004/08/24 eat -

       Doesn't seem like this next twist should be needed.  It would
       only have value if a compute method can return a dynamic
       object.  In that case, I would think we should compute again,
       instead of just taking the value.
    */
    if(object->value!=object){
      obj=object;
      object=grabObject(object->value);
      dropObject(obj);
      }  
    entry->object=object;
    }
  return(useList(head));
  }

/*
*  Parse an object list expression and generate an object list structure
*/
NB_Link *newMember(NB_Term *context,char **cursor){
  NB_Object *object;
  NB_Link *member=NULL,*entry,**next;
  char symid=',',token[256];
  
  next=&member;
  while(symid==','){
    if(NULL==(object=nbParseCell(context,cursor,0))) object=nb_Placeholder;
    if((entry=nb_LinkFree)==NULL) entry=nbAlloc(sizeof(NB_Link));
    else nb_LinkFree=entry->next;
    *next=entry;
    entry->next=NULL;
    next=&(entry->next);
    entry->object=grabObject(object);
    symid=nbParseSymbol(token,sizeof(token),cursor);
    }
  return(member);
  }

/*
*  Drop an object list
*/
void *dropMember(NB_Link *member){
  NB_Link *entry,*last;
  if(member==NULL) return(NULL);
  for(entry=member;entry!=NULL;entry=entry->next){
    dropObject(entry->object);
    entry->object=NULL;
    last=entry;
    }
  last->next=nb_LinkFree;
  nb_LinkFree=member;
  return(NULL);
  }

/*
*  Hash a string and return a pointer to a pointer in the hashing vector.
*
*  Note: The hashing algorithm can be improved for long strings
*
*/
uint32_t hashList(NB_Link *member){
  unsigned long h=0;
  unsigned long *m;
  uint32_t hashcode;

  for(;member!=NULL;member=member->next){
    m=(unsigned long *)&(member->object);
    h=((h<<3)+*m);
    }
  hashcode=h;
  return(hashcode);
  }

/*
*  Use an existing list or create a new one.
*/
NB_List *useList(NB_Link *member){
  NB_Link *mbr1,*mbr2;
  NB_List *list,**listP;
  NB_Hash *hash=nb_ListType->hash;
  int level,maxlevel=0;
  uint32_t hashcode;
 
  if(member==NULL) return(nb_ListNull);
  hashcode=hashList(member);
  listP=(NB_List **)&(hash->vect[hashcode&hash->mask]);
  for(;*listP!=NULL && (*listP)->link->object<member->object;listP=(NB_List **)&((*listP)->cell.object.next));
  for(;*listP!=NULL && (*listP)->link->object==member->object;listP=(NB_List **)&((*listP)->cell.object.next)){
    mbr1=(*listP)->link->next;
    mbr2=member->next;
    while(mbr1!=NULL && mbr2!=NULL && mbr1->object==mbr2->object){
      mbr1=mbr1->next;
      mbr2=mbr2->next;
      }
    if(mbr1==NULL && mbr2==NULL){   /* found it, return temp list to free list */
      for(mbr1=member;mbr1!=NULL;mbr1=mbr1->next){
        mbr1->object=dropObject(mbr1->object);
        mbr2=mbr1;
        }
      mbr2->next=nb_LinkFree;
      nb_LinkFree=member;
      return(*listP);                 /* return existing list object */
      }
    }
  /* didn't find it, get a new one */
  list=nbCellNew(nb_ListType,(void **)&nb_ListFree,sizeof(NB_List));
  list->cell.object.hashcode=hashcode;
  list->cell.object.value=nb_Disabled;
  list->link=member; 
  list->cell.object.next=(NB_Object *)*listP;  /* insert in hash list */
  *listP=list;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&nb_ListType->hash);
  /* set function level */
  for(;member!=NULL;member=member->next){
    if(member->object->value!=member->object &&
       (level=((NB_Cell *)member->object)->level)>maxlevel) maxlevel=level;
    } 
  list->cell.level=maxlevel+1;
  return(list);
  }

// 2005/05/01 eat - modified
//
// Parse a list starting just after the open paren "(", or "[", or whatever.
// We allow the list to be terminated by anything that terminates a cell
// expression other than comma ",".  It is up to the caller to check the
// terminator symbol and drop the list.
// 
//   ()      - empty list - list with no members
//   (_)     - list with one placeholder member
//   (_,_)   - list with two placeholder members
//   (,)     - list with two placeholder members (alternate syntax)
//   (a,_,b) - list whose second member is placeholder
//   (a,,b)  - list whose second member is placeholder (alternate syntax)
// 
//
NB_List *parseList(NB_Term *context,char **cursorP){
  NB_Link *member=NULL;
  char *cursor=*cursorP;

  while(*cursor==' ') cursor++;
  if(*cursor!=')') member=newMember(context,&cursor);
  *cursorP=cursor;
  return(useList(member));
  }

void destroyList(NB_List *list){
  NB_List **listP;
  NB_Hash *hash=nb_ListType->hash;
  uint32_t hashcode;

  //hashcode=hashList(list->link);
  hashcode=list->cell.object.hashcode;
  listP=(NB_List **)&(hash->vect[hashcode&hash->mask]);
  for(;*listP!=NULL && *listP!=list;listP=(NB_List **)&((*listP)->cell.object.next));
  if(*listP!=NULL)
    *listP=(NB_List *)list->cell.object.next;    /* remove from hash list */
  list->link=dropMember(list->link);
  list->cell.object.next=(NB_Object *)nb_ListFree;   /* insert in free list */
  nb_ListFree=list;
  }

void printMembers(NB_Link *member){
  if(member==NULL) return;
  printObject(member->object);
  for(member=member->next;member!=NULL;member=member->next){
    outPut(",");
    printObject(member->object);
    }
  }

void printList(NB_List *list){
  NB_Link *member;

  outPut("(");
  for(member=list->link;member!=NULL;member=member->next){
    printObject(member->object);
    if(member->next!=NULL) outPut(",");
    }
  outPut(")");
  }

void nbListShowAll(void){
  nbHashShow(nb_ListType->hash,"Lists",NULL);
  }

/*
*  Solve a list function.
*
*    A list is Unknown if any member is Unknown.
*
*/
void solveList(NB_List *list){
  NB_Link *member;

  if(trace) outMsg(0,'T',"solveList: called");
  for(member=list->link;member!=NULL;member=member->next){
    if(member->object->value==nb_Unknown){
      nbCellSolve_((NB_Cell*)member->object);
      if(member->object->value==nb_Unknown || list->cell.object.value!=nb_Unknown) return;
      }
    }
  return;
  }

/*
*  Evaluate a list function.
*
*    If the value of any member object is Unknown, the value of the
*    list is unknown.
*
*    The value toggles between True and False if the member objects
*    are known.  This ensures that the subscribers are notified when
*    the value of any member object is changed.
*/
NB_Object *evalList(NB_List *list){
  NB_Link *member;
 
  if(trace) outMsg(0,'T',"evalList(): called");
  for(member=list->link;member!=NULL;member=member->next){
    if(member->object->value==nb_Unknown) return(nb_Unknown);
    }
  if(list->cell.object.value==NB_OBJECT_TRUE) return(NB_OBJECT_FALSE);
  if(list->cell.object.value!=nb_Unknown) list->cell.object.value=nb_Unknown; /* force change */
  if(trace) outMsg(0,'T',"evalList(): returning True");
  return(NB_OBJECT_TRUE);
  }

void enableList(NB_List *list){
  NB_Link *member;

  if(trace) outMsg(0,'T',"enableList: called");
  for(member=list->link;member!=NULL;member=member->next){
    nbAxonEnable((NB_Cell *)member->object,(NB_Cell *)list);
    }
  }

void disableList(NB_List *list){
  NB_Link *member;

  if(trace) outMsg(0,'T',"disableList() called");
  for(member=list->link;member!=NULL;member=member->next){
    nbAxonDisable((NB_Cell *)member->object,(NB_Cell *)list);
    }
  if(trace) outMsg(0,'T',"disableList() returning");
  }

/*
*  Initialize the list hash
*    Must be called before parseList
*/
void nbListInit(NB_Stem *stem){
  if(trace) outMsg(0,'T',"listInit: called");
  nb_ListType=newType(stem,"list",NULL,0,printList,destroyList);
  nb_ListType->apicelltype=NB_TYPE_LIST;
  nbCellType(nb_ListType,solveList,evalList,enableList,disableList);
  nb_ListNull=nbCellNew(nb_ListType,(void **)&nb_ListFree,sizeof(NB_List));
  nb_ListNull->cell.object.value=nb_Disabled;
  nb_ListNull->link=NULL;
  nb_ListNull->cell.object.next=NULL;  /* don't insert in hash list */
  nb_ListNull->cell.object.refcnt=-1;  // don't give it up
  }

/***************************************************************************
* API Functions 
***************************************************************************/

/*
*  Make a cell list
*/
nbCELL nbListCreate(nbCELL context,char **cursorP){
  NB_List *list;
  if(cursorP==NULL){
    list=nbCellNew(nb_ListType,(void **)&nb_ListFree,sizeof(NB_List));
    list->link=NULL;
    }
  else list=parseList((NB_Term *)context,cursorP); 
  return((nbCELL)list);
  }

/*
*  Get pointer to first cell in a cell list
*
*    returns NULL if the list is empty
*/
void *nbListOpen(nbCELL context,nbCELL list){
  if(list==NULL || list->object.type!=nb_ListType || ((NB_List *)list)->link==NULL) return(NULL);
  return(&((NB_List *)list)->link);
  }

/*
*  Insert a cell in a cell list
*/
void *nbListInsertCell(nbCELL context,nbSET *setP,nbCELL cell){
  NB_Link *link;
  if((link=nb_LinkFree)==NULL) link=nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=link->next;
  link->next=*setP;
  link->object=(NB_Object *)cell;
  *setP=link;
  return(&(link->next));
  }

/*
*  Remove an item from a list 
*/
NB_Cell *nbListRemoveCell(nbCELL context,nbSET *setP){
  NB_Link *link;
  NB_Cell *cell;
  if(setP==NULL || (link=*setP)==NULL) return(NULL);
  cell=(NB_Cell *)link->object;
  *setP=link->next;
  link->next=nb_LinkFree;
  nb_LinkFree=link;
  return(cell);
  }

/*
*  Get next object from a list  [short for nbGetListNext() and nbGetListObject()]
*/ 
NB_Cell *nbListGetCell(nbCELL context,nbSET *setP){
  NB_Link **linkP=*setP;
  NB_Cell *cell;
  if(linkP==NULL || *linkP==NULL) return(NULL);
  cell=grabObject((*linkP)->object);
  *setP=&(*linkP)->next;  /* step to next link */
  return(cell);
  }

/*
*  Get next cell value from a cell list
*
*  NOTE: Cells are returned with a grab, so the caller is responsible for
*  issuing an nbCellDrop() when no longer using the cell.
*/

NB_Cell *nbListGetCellValue(nbCELL context,nbSET *setP){
  NB_Cell *cell;
  NB_Link **linkP=*setP;

  if(linkP==NULL || *linkP==NULL) return(NULL);
  cell=(NB_Cell *)(*linkP)->object;
  *setP=&(*linkP)->next;  /* step to next link */
  if((NB_Object *)cell->object.value==nb_Disabled) cell=(NB_Cell *)cell->object.type->compute(cell);
  else cell=grabObject((NB_Cell *)cell->object.value); /* 2004/08/28 eat - grab moved here from return */
  return(cell);
  }

void nbListEmpty(nbCELL context,nbSET *set){
  *set=dropMember(*set);
  }
