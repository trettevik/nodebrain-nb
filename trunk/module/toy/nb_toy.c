/*
* Copyright (C) 2003-2012 The Boeing Company
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
* File:     nb_toy.c
*
* Title:    Toy Node Module - Example
*
* Function:
*
*   This file is a node module example illustrating how you can
*   extend NodeBrain by adding support for new types of nodes.
* 
* Reference:
*
*   See "NodeBrain API Reference" for more information.
*
* Examples:
*
*   The toy module provides three trivial skills and illustrates how to use
*   the nbBind() function to declare skills that don't conform to the naming
*   standards. 
*
*     nb_toy.c  - Trival skill examples: sum, add, and count
*
*   For more complicated examples, refer to any of the node modules in the
*   distribution.
*
*     nb_<name>.c  - Other examples.
*   
* Syntax:
*
*   Module Declaration:  (normally not required)
*
*     declare <module> module <skill_module>
*
*     declare toy  module /usr/local/lib/nb_toy.so;       [Linux and Solaris]
*     declare toy  module nb_toy.dll;               [Windows]
*
*   Skill Declaration:   (normally not required)
*
*     declare <skill> skill <module>.<symbol>[(args)][:text]
*
*     declare sum     skill toy.sum;
*     declare count   skill toy.count;
*
*   Node Definition:
*
*     define <term> node <skill>[(args)][:text]
*
*     define sum node sum;
*     # The "toy.add" skill is defined by the nbBind() function
*     define add node toy.add;
*     define minuteCounter node count(~(60s));
*     define aisoneCounter node count(a=1);
*
*   Cell Expression:
*
*     ... <node>[(args)] ...
*
*     define r1 on(sum(a,b)>2 and add(a,b)<10):...
*     define r1 on(minuteCounter<5 and aisoneCounter>3):...
*
*   Assertion:
*
*     assert <node>(args) ...
*
*     assert minuteCounter()=0,aisoneCounter()=-3;
*
*   Disable:
*
*     disable <node>
*
*     disable minuteCounter;
*
*   Enable:
*
*     enable <node>
*
*     enable minuteCounter;
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003-12-15 Ed Trettevik - first tinkering
* 2004-04-06 eat 0.6.0  added comments
* 2005-04-09 eat 0.6.2  adapted to API changes
* 2005-05-14 eat 0.6.3  countBind() and addBind() updated for moduleHandle
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=============================================================================
*/
#include "config.h"
#include <nb.h>

#if defined(_WINDOWS)
BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved){
	switch(fdwReason){
	  case DLL_PROCESS_ATTACH: break;
	  case DLL_THREAD_ATTACH: break;
	  case DLL_THREAD_DETACH: break;
	  case DLL_PROCESS_DETACH: break;
	}
  return(TRUE);
  }
#endif

/*
*========================================================================
* time() skill example:
*
* Description:
*
*   The time skill simply returns the time in number of seconds since
*   the epoch.
*
*       define time node toy.time;
*
*       assert eventTime=time();
*
*   This skill provides only an Evaluate method, so it is not capable
*   of commands and assertions.
*
*========================================================================
*/

/* Evaluation Method */

nbCELL timeEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  double r=(double)time(NULL);
  return(nbCellCreateReal(context,r));
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern
#endif
void *timeBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,timeEvaluate);
  return(NULL);
  }

/*
*========================================================================
* padIpAddr() skill example:
*
* Description:
*
*   The padIpAddr skill converts an IP address string from normal form
*   to a form where each number is padded with leading zeros to force
*   three digits.  This form is handy for sorting and comparing.
*
*       define padIpAddr node toy.padIpAddr;
*       define r1 on(padIpAddr(fromIp)>"012.020.127.010");
*
*       assert fromIp="130.42.7.20";
*
*       Value of padIpAddr(fromIp) is "130.042.007.020"
*
*   This skill provides only an Evaluate method, so it is not capable
*   of commands and assertions.
*
*========================================================================
*/

/* Evaluation Method */

nbCELL padIpAddrEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  nbCELL cell;
  void *ptr;
  int type,i,len,b[4];
  char *strIn,*period,strNum[4],ipAddr[16];

  ptr=nbListOpen(context,arglist);
  if(ptr==NULL) return(NB_CELL_UNKNOWN);
  cell=nbListGetCellValue(context,&ptr);
  if(cell==NULL) return(cell);
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING) return(cell);
  strIn=nbCellGetString(context,cell);
  nbCellDrop(context,cell);
  for(i=0;i<3;i++){
    period=strchr(strIn,'.');
    if(period==NULL || period-strIn>3) return(cell);
    len=period-strIn;
    strncpy(strNum,strIn,len); 
    *(strNum+len)=0;
    b[i]=atoi(strNum);
    strIn+=len+1;
    }
  len=strlen(strIn);
  if(len>3) return(cell);
  strncpy(strNum,strIn,len);
  *(strNum+len)=0;
  b[3]=atoi(strNum);
  sprintf(ipAddr,"%3.3d.%3.3d.%3.3d.%3.3d",b[0],b[1],b[2],b[3]);
  return(nbCellCreateString(context,ipAddr));
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern
#endif
void *padIpAddrBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,padIpAddrEvaluate);
  return(NULL);
  }


/*
*========================================================================
* sum() skill example:
*
* Description:
*   
*   The sum() skill adds real number parameters and string parameter
*   lengths and returns the sum.  Arguments of other types are ignored;
*   that is, do not contribute to the sum.  There is no special handling
*   of the Unknown value, it simply doesn't contribute to the sum.
*
*       declare toy module /usr/local/lib/nb_toy.so;
*       declare sum skill toy.sum;
*       define sum node sum;
*       define r1 on(sum(a,b,c)>20);
*
*       assert a=1,b=2,c=?,x=a+b+c,y=sum(a,b,c);
*
*       Value of x is ?.
*       Value of Sum(a,b,c) is 3.
*
*   This skill module provides only an Evaluate method, so the sum node
*   is not capable of commands and assertions as follows.
*
*       +Sum:...            # This will not work
*       assert Sum(1,3)=4;  # This will not work
*
*========================================================================
*/

/* Evaluation Method */

nbCELL sumEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  nbCELL cell;
  void *ptr;
  double r=0;
  int type;

  ptr=nbListOpen(context,arglist);
  while((cell=nbListGetCellValue(context,&ptr))!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_REAL) r+=nbCellGetReal(context,cell);
    else if(type==NB_TYPE_STRING) r+=strlen(nbCellGetString(context,cell));
    }
  return(nbCellCreateReal(context,r));
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern 
#endif
void *sumBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,sumEvaluate);
  return(NULL);
  }

/* 
*========================================================================
* add() skill example:
*
* Description:
*
*   This example illustrates how the nbBind() function can be used
*   to declare skills automatically, where the function names
*   don't necessarily match the naming assumptions.  To keep it
*   simple, we declare a skill called "add" that is an alias
*   for "sum".  It is an alias because we use the sum() function
*   for skill initialization (second argument to nbSkillDeclare).
*   We could have just as easily pointed to a unique skill 
*   initialization method.  From there we could use the
*   nbSkillSetMethod() function to share the sumEvalute() method
*   with "sum", but perhaps include other methods as well.
*   
*     declare sample module /user/local/lib/nb_toy.dll;
*     define total node sample.add;
*     define r1 on(total(a,b,c)>20);
*
*========================================================================
*/

/* Module Initialization Function */

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *nbBind(nbCELL context,char *ident,nbCELL arglist,char *text){
  // nbLogMsg(context,0,'T',"nbBind() called for \"%s\".",ident);
  nbSkillDeclare(context,sumBind,NULL,ident,"add",arglist,text);
  return(NULL);
  }

/*
*========================================================================
* count() skill method:
*
* Description:
*
*   The count skill illustrates the enable and disable methods used
*   to subscribe and unsubscribe to the value of other cells.  This
*   example also uses a "node handle" to maintain node specific
*   information.  This is created by the countConstruct() method.  A
*   countAssert() method is included to set a counter value.
*
*       declare sample module /usr/local/lib/nb_toy.so;
*       declare count skill sample.count;
*
*       define minuteCounter node count(~(60s));
*       define aisoneCounter node count(a=1);  
*       define r1 on(minuteCounter<5 and aisoneCounter>3):...     
*
*       assert a=1;
*       assert a=0;
*       assert a=1;
*       assert a=0;
*       assert a=1; # rule r1 fires if done within 5 minutes
*
*       assert minuteCounter()=0,aisoneCounter()=0;  # start over
*
*========================================================================
*/

/* Skill handle structure - unique to count() skill */

struct NB_MOD_COUNTER{
  long count;          /* count of times the condition has been true */
  nbCELL cell;       /* condition to count */
  int  isTrue;         /* 1 - true, 0 - false or unknown */
  };

typedef struct NB_MOD_COUNTER NB_MOD_Counter;

/*
*  counstruct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define aisoneCounter node count(a=1);
*/
void *countConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  struct NB_MOD_COUNTER *counter;
  nbCELL cell=NULL;
  nbSET argSet;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCell(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"One argument cell expression required");
    return(NULL);
    }
  counter=nbAlloc(sizeof(NB_MOD_Counter));
  counter->count=0;
  counter->cell=cell;
  counter->isTrue=0;
  return(counter);
  }

/*
*  assert() method
*
*    assert <node>(<args>)=<value>
*
*    assert aisoneCounter()=0;
*/
int countAssert(nbCELL context,void *skillHandle,NB_MOD_Counter *counter,nbCELL arglist,nbCELL value){
  int cellType;
  char *counterName;

  cellType=nbCellGetType(context,value);
  if(cellType==NB_TYPE_REAL){
    counter->count=(int)nbCellGetReal(context,value);
    nbNodeSetValue(context,nbCellCreateReal(context,counter->count));
    }
  else{
    counterName=nbTermGetName(context,context);
    nbLogMsg(context,0,'E',"Counter %s expects real value assertion",counterName);
    return(-1);
    }
  return(0);
  }

/*
*  evaluate() method
*
*    ... <node>[(<args>)] ...
*
*    define r1 on(aisoneCount>5);
*/
nbCELL countEvaluate(nbCELL context,void *skillHandle,NB_MOD_Counter *counter,nbCELL arglist){
  double r=counter->count;
  nbCELL argCell,*expVal;
  void *pointer;
  int cellType;
  char *counterName;

  pointer=nbListOpen(context,arglist);
  argCell=nbListGetCellValue(context,&pointer);
  /* the arglist is empty when we are asked to re-evaluate the counter */
  if(argCell==NULL){
    expVal=nbCellGetValue(context,counter->cell);
    if(expVal!=NB_CELL_UNKNOWN && expVal!=NB_CELL_FALSE){
      if(counter->isTrue==0){
        counter->count++;
        counter->isTrue=1;
        }
      }
    else counter->isTrue=0;
    r=counter->count;
    }
  else{ /* divide by argument if specified */
    cellType=nbCellGetType(context,argCell);
    if(cellType==NB_TYPE_REAL) r/=nbCellGetReal(context,argCell);
    else{
      counterName=nbTermGetName(context,context);
      nbLogMsg(context,0,'E',"Counter %s expects real argument",counterName);
      r=0;
      }
    nbCellDrop(context,argCell);
    }
  return(nbCellCreateReal(context,r));
  }

/*
*  enable() method
*
*    enable <node>
*
*    enable aisoneCounter;
*/
int countEnable(nbCELL context,void *skillHandle,NB_MOD_Counter *counter){
  nbCELL node;
  node=nbTermGetDefinition(context,context);
  nbCellEnable(counter->cell,node);
  nbNodeSetLevel(context,counter->cell);
  /* nbCellEnable(context,counter->cell);*/
  return(0);
  }

/*
*  disable method
*  
*    disable <node>
*
*    disable aisoneCounter;
*/
int countDisable(nbCELL context,void *skillHandle,NB_MOD_Counter *counter){
  nbCELL node;
  node=nbTermGetDefinition(context,context);
  nbCellDisable(counter->cell,node);
  /* nbCellDisable(context,counter->cell);*/
  return(0);
  }

/* 
*  show() method
*
*    show <node>
*
*    show aisoneCounter;
*/
int countShow(nbCELL context,void *skillHandle,NB_MOD_Counter *counter,int option){
  /* if(option!=NB_SHOW_REPORT) return(0); */
  switch(option){
    case NB_SHOW_ITEM: 
      nbLogPut(context,"(");
      nbCellShow(context,counter->cell);
      nbLogPut(context,")");
      break;
    case NB_SHOW_REPORT: nbLogPut(context,"counter %d\n",counter->count); break;
    }
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*    undefine r1;
*    undefine aisoneCounter;
*/
int countDestroy(nbCELL context,void *skillHandle,NB_MOD_Counter *counter){
  nbLogMsg(context,0,'T',"counterDestroy called");
  if(counter->cell!=NULL) nbCellDrop(context,counter->cell);
  nbFree(counter,sizeof(NB_MOD_Counter));
  return(0);
  }

/*
*  skill initialization method
*
*  This method is used to associate method functions with a skill.  It is also
*  possible to create a skill handle (not to be confused with a node handle
*  created by the construct() method).  You would use a skill handle in a
*  case where arguments are passed to this routine to "customize" a skill.  The
*  customization options would be stored in a structure pointed to by the skill
*  handle, and the skill handle would be returned.  Another reason for having
*  a skill handle is to share gathered knowledge between all nodes of a given
*  skill.  For example, a skill initialization method could read information
*  from a file to be shared by all nodes associated with the skill.
*  
*/
#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern 
#endif
void *countBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,countAssert);
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,countConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,countDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,countEnable);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,countEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,countShow);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,countDestroy);
  return(NULL);
  }
