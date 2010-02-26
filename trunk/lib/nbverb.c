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
* File:     nbverb.h 
*
* Title:    Verb Object Methods
*
* Function:
*
*   This file provides methods for NodeBrain VERB objects used internally
*   to parse commands.  The VERB type extends the OBJECT type defined in
*   nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initVerb();
*   struct VERB *newVerb();
*   void printVerb();
*   void printVerbAll();
* 
* Description
*
*   A verb object represents a verb within the NodeBrain language.  Verbs
*   are defined at initialization time only---verbs are never destroyed and
*   the language does not provide for user defined verbs.  The language is
*   extended only via node module commands.
*  
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/11/22 Ed Trettevik (original prototype version introduced in 0.6.4)
*=============================================================================
*/
#include "nbi.h"

void nbVerbPrint(struct NB_VERB *verbEntry){
  char verb[sizeof(verbEntry->verb)];
  strcpy(verb,verbEntry->verb);
  if(strlen(verb)<sizeof(verb)-1){
    strncat(verb,"                         ",sizeof(verb)-1-strlen(verb));
    *(verb+sizeof(verb)-1)=0;
    }
  outPut("%s %s\n",verb,verbEntry->syntax);
  }

void nbVerbPrintSub(struct NB_VERB *verbEntry){
  if(verbEntry->lower!=NULL) nbVerbPrintSub(verbEntry->lower);
  nbVerbPrint(verbEntry);
  if(verbEntry->higher!=NULL) nbVerbPrintSub(verbEntry->higher);
  }

void nbVerbPrintAll(struct NB_VERB *verbEntry){
  outPut("Verb Table:\n");
  nbVerbPrintSub(verbEntry);
  }

int nbVerbDefine(NB_Stem *stem,char *verb,int authmask,int flags,void (*parse)(struct NB_CELL *context,char *verb,char *cursor),char *syntax){
  struct NB_VERB *newVerb;
  
  newVerb=malloc(sizeof(struct NB_VERB));
  strcpy(newVerb->verb,verb);   // depend on caller not to pass verb over 15 characters
  newVerb->authmask=authmask;
  newVerb->flags=flags;
  newVerb->parse=parse;
  newVerb->syntax=syntax;
  newVerb->lower=stem->verbTree;
  newVerb->higher=NULL;
  stem->verbTree=newVerb;
  stem->verbCount++;
  //outMsg(0,'T',"verb created - %s\n",verb);
  return(0);
  }

struct NB_VERB *nbVerbFind(struct NB_VERB *verbEntry,char *verb){
  int cmp;
  //outMsg(0,'T',"nbVerbFind called - %s",verb);
  while(verbEntry!=NULL){
    //outMsg(0,'T',"  checking %s",verbEntry->verb);
    if((cmp=strcmp(verb,verbEntry->verb))<0) verbEntry=verbEntry->lower;
    else if(cmp>0) verbEntry=verbEntry->higher;
    else return(verbEntry); 
    }
  //outMsg(0,'t',"nbVerbFind returning - %s",verbEntry->verb);
  return(NULL);
  }

/*
*  Balance the tree - it is initially just a list
*/
struct NB_VERB *nbVerbBalance(struct NB_VERB *verbHigh,int n){
  // insert code to balance the tree
  struct NB_VERB *verb=verbHigh;
  int l,h,i;
  if(n<1) return(NULL);
  l=n/2;
  h=n-l-1;
  i=h;
  while(i>0){
    verb=verb->lower;
    i--;
    }     
  verb->lower=nbVerbBalance(verb->lower,l);
  verb->higher=nbVerbBalance(verbHigh,h);
  return(verb);
  }
