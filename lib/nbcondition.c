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
* File:     nbcondition.c 
*
* Title:    Condition Object Routines (prototype)
*
* Function:
*
*   This header provides routines for nodebrain condition objects.  These
*   routines are called by the command interpreter.
*
* Synopsis:
*
*   #include "nb.h"
*   
*   ... many functions ...
*
* Description
*
*   You can then construct conditions using the useCondition() method.
*
*     struct COND *mycond=useCondition(not,type,left,right);
*
*   The useCondition method will reserve left and right with a call to
*   grabObject() if a new condition is generated.  Otherwise, an existing
*   condition is returned and reference count for left and right is not
*   altered.  You must call grabObject to reserve the returned condition
*   for any references you assign.
*
*     mystructPtr=grabObject(mycond);
*
*   When a reference is no longer needed, call dropObject to decrement the
*   the reference count.  When the reference count decrements to zero the
*   condition object will be reused.
*
*     dropObject(mycond);
*
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/07/01 Ed Trettevik (original prototype version)
*             1) This code is a revision to code previously imbedded
*                in nodebrain.c. The primary difference is the use of nbobj.h
*                routines.
* 2001/07/09 eat - version 0.2.8
*             1) The use count of subordinate objects is now incremented by
*                calls to grabObject() when useCondition sets the left and
*                right object pointers.
* 2002/08/19 eat - version 0.4.0
*             1) Modified evalCacheStr to return Unknown when the string
*                variable is unknown, even if cache is empty.
*                      
*                      person{name}   [unknown if name is unknown]
* 2002/08/28 eat - version 0.4.0 A7
*             1) The Cache membership condition has been reworked to support
*                list objects.
* 2002/09/03 eat - version 0.4.1 A1
*             1) Started modifying relational operator functions to support
*                real numbers in addition to strings.  At the same time we
*                need to reduce the number of functions by handling any
*                combination of simple objects and function objects.  We should
*                consider data structure changes to keep the logic fast.
* 2002/09/22 eat - version 0.4.1 A4
*             1) Included a few new operators
*
*                ?e   - Unknown
*                !?e  - Known
*                !!e  - True
*                []e  - Closed World Value (? is 0)
*                !&   - Nand
*                !|   - Nor
*
* 2002/09/25 eat - version 0.4.1 A4
*             1) Included >= and <= so we don't have to implement them with !.
*
* 2004/10/02 eat 0.6.1  Reworked solve methods to remove evaluation logic
*                When terms are defined during a "solve" command, their new
*                values are published and nbCellReact() handles the calls to
*                eval methods to re-evaluate.  So it is no longer necessary
*                to include "evaluation" logic in solve methods.  They simply
*                need to call nbCellSolve_() for each Unknown until a value
*                is known.
*
* 2004/10/02 eat 0.6.1  Partially implemented ~~ on Windows
*                On Windows, "~~" now means "contains".  This means a vary
*                simple regular expression is supported.  This was done not
*                so much for the functionality, but to open up more of code
*                on Windows for testing---in preparation for fully supporting
*                regular expressions later.
*
*                    name~~"abc"  
*
* 2006/10/28 eat 0.6.6  Fixed a bug in alertRule() causing on/when rules to fire in error
*                An on/when rule may be alerted to change of value in the condition
*                from one true value to another.  We were not checking for this.  
* 
*                    define r1 on(a);
*                    assert a=1;  # rule fires
*                    assert a=2;  # rule was firing but won't now
*
*                This bug was not detected by our applications because we had no rule
*                conditions of this type.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-10-17 eat 0.8.12 Replaced termGetName with nbTermName
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-11 eat 0.8.13 Checker updates
* 2013-12-09 eat 0.9.00 Replaced condTypeNode with nb_SentenceType objects
*            This was necessary to add more fields to a sentence (previously
*            called a node condition.
* 2014-01-12 eat 0.9.00 Removed hash pointer - referenced via types
* 2014-01-18 eat 0.9.00 Implementing LT and GT axon
* 2014-04-06 eat 0.9.01 Assume True -? replaces [] closed world
* 2014-04-06 eat 0.9.01 Assume False +? included
* 2014-04-06 eat 0.9.01 Old AND operator "&" returns 1 for true
* 2014-04-06 eat 0.9.01 New AND-like operator "true" returns multiple true values
* 2014-04-06 eat 0.9.01 Old OR operator "|" returns 1 for true
* 2014-04-06 eat 0.9.01 New OR-like  operator "untrue" returns multiple true values
* 2014-04-27 eat 0.9.01 The "not" operator is now deprecated to avoid reserved terms
* 2014-04-27 eat 0.9.01 Fixed a time delay bug
*            Multiple types of delays of the same duration on the same condition
*            cause conflicts in the management of a subordinate time condition.
*            Modified to eliminate subordinate time condition and now manage
*            state of the time delay using a flag in the cell.mode field.
*=============================================================================
*/
#include <nb/nbi.h>

struct ACTION *actList;  /* action list (proactive or reactive THEN) */
struct ACTION *ashList;  /* list of rules that fired */

struct COND *condFree=NULL;

struct TYPE *condTypeNerve;    /* nerve() - nerve cells simply report new values */
struct TYPE *condTypeOnRule;   /* on()    - on(cond,action) - asserts and alerts */
struct TYPE *condTypeWhenRule; /* when()  - when(cond,action) - one time on() rule */
struct TYPE *condTypeIfRule;   /* if()    - if(cond,action) - alerts */
struct TYPE *condTypeNot;      /* !c      - not(cond) */
struct TYPE *condTypeTrue;     /* !!c     - true(cond) - All true values compression to 1 */
struct TYPE *condTypeUnknown;  /* ?c      - unknown(cond) */
struct TYPE *condTypeKnown;    /* !?c     - known(cond) */
struct TYPE *condTypeAssumeFalse;  // -?c - assumeFalse(cond) ?=>!
struct TYPE *condTypeAssumeTrue;   // +?c - assumeTrue(cond) ?=>1
struct TYPE *condTypeDefault;      /* c?d     - default(cond,cond) */
struct TYPE *condTypeLazyAnd;      /* c&&d    - lazyand(cond,cond) */
struct TYPE *condTypeAnd;          /* c&d     - and(cond,cond) */
struct TYPE *condTypeNand;         /* c!&d    - nand(cond,cond) */
struct TYPE *condTypeLazyOr;       /* c||d    - lazyor(cond,cond) */
struct TYPE *condTypeOr;           /* c|d     - or(cond,cond) */
struct TYPE *condTypeNor;          /* c!|d    - nor(cond,cond) */
struct TYPE *condTypeXor;          /* c|!&d   - xor(cond,cond) */
struct TYPE *condTypeAndMonitor;  /* c &~& d  - andMonitor(cond,cond)*/
struct TYPE *condTypeOrMonitor;   /* c |~| d  - orMonitor(cond,cond)*/
struct TYPE *condTypeAndCapture;  /* c &^& d  - andCapture(cond,cond)*/
struct TYPE *condTypeOrCapture;   /* c |^| d  - andCapture(cond,cond)*/
struct TYPE *condTypeFlipFlop;    /* c ^ d    - flipflop(cond,cond)*/
struct TYPE *condTypeRelEQ;       /* a=b     - compare(object,object) */
struct TYPE *condTypeRelNE;       /* a<>b    - compare(object,object) */
struct TYPE *condTypeRelLT;       /* a<b     - compare(object,object) */
struct TYPE *condTypeRelLE;       /* a<=b    - compare(object,object) */
struct TYPE *condTypeRelGT;       /* a>b     - compare(object,object) */
struct TYPE *condTypeRelGE;       /* a>=b    - compare(object,object) */
struct TYPE *condTypeTime;         /* ~()    - timespec */
struct TYPE *condTypeDelayTrue;    /* ~^()   - logic part of delay(cond,timespec)*/
struct TYPE *condTypeDelayFalse;   /* ~^!()  - logic part of delay(cond,timespec)*/
struct TYPE *condTypeDelayUnknown; /* ~^?()  - logic part of delay(cond,timespec)*/
struct TYPE *condTypeMatch;        /* a~"..."  - match(term,regexp) */
struct TYPE *condTypeChange;       /* a~~      - change(term) */

/*struct TYPE *condTypeCache; */        /* c(<list>)- exists(cache,list) */

void condSchedule();
void condUnschedule();

/**********************************************************************
* Support Routines
*/
/*
*  Hash a condition and return a pointer to a pointer in the hash table.
*/
uint32_t hashCondOld(struct TYPE *type,void *left,void *right){
  //unsigned long h,*t,*l,*r;
  unsigned long h,*l,*r;
  //t=(unsigned long *)&type;
  l=(unsigned long *)&left;
  r=(unsigned long *)&right;
  //h=(*l>>3)+(*r>>3)+(*t>>3);
  h=((*l>>3)+(*r>>3));
  h+=(h>>3);
  //h^=(h>>8);
  return((uint32_t)h);
  }

uint32_t hashCond(struct TYPE *type,void *left,void *right){
  unsigned long h,*l,*r;
  uint32_t key,*k1,*k2;
  l=(unsigned long *)&left;
  r=(unsigned long *)&right;
  h=(*l>>3);
  h+=(h<<10);
  h^=(h>>6);
  h+=(*r>>3);
  h+=(h<<10);
  h^=(h>>6);
  k1=(uint32_t *)&h;
  k2=k1+1;
  key=*k1+*k2;
  //key+=(key<<10);
  //key^=(key>>6);
  //key+=*k2;
  //key+=(key<<10);
  //key^=(key>>6);
  return(key);
  }


// This algorithm is based on Perl.
// It is the "One-at-a-time" algorithm by Bob Jenkins (http://burtleburtle.net/bob/hash/doobs.html)
#define NB_HASH_PTR(hash,ptr,len){ \
  register const char * const s_PeRlHaSh_tmp = (char *)&ptr; \
  register const unsigned char *s_PeRlHaSh = (const unsigned char *)s_PeRlHaSh_tmp; \
  register int32_t i_PeRlHaSh = len; \
  register uint32_t hash_PeRlHaSh = hash; \
  while(i_PeRlHaSh--) { \
    hash_PeRlHaSh += *s_PeRlHaSh++; \
    hash_PeRlHaSh += (hash_PeRlHaSh << 10); \
    hash_PeRlHaSh ^= (hash_PeRlHaSh >> 6); \
    } \
  hash_PeRlHaSh += (hash_PeRlHaSh << 3); \
  hash_PeRlHaSh ^= (hash_PeRlHaSh >> 11); \
  (hash) = (hash_PeRlHaSh + (hash_PeRlHaSh << 15)); \
  }

uint32_t hashCondNew(struct TYPE *type,void *left,void *right){
  uint32_t key=0;
  NB_HASH_PTR(key,left,sizeof(left))
  NB_HASH_PTR(key,right,sizeof(right))   
  return(key);
  }

/**********************************************************************
* Object Management Methods
*  This type requires no special object management methods.
**********************************************************************/
/*
*  Condition Print Methods
*/
void condPrintNerve(struct COND *cond){
  outPut(" %s ",cond->cell.object.type->name);
  printObject(cond->left);
  } 
     
void condPrintRule(struct COND *cond){
  struct ACTION *action;
  int paren=0;

  if(((NB_Object *)cond->left)->value==cond->left ||
    ((NB_Object *)cond->left)->type==termType ||
    ((NB_Object *)cond->left)->type==nb_SentenceType ||
    ((NB_Object *)cond->left)->type==condTypeTime) paren=1;
  outPut("%s",cond->cell.object.type->name);
  if(paren) outPut("(");
  printObject(cond->left);
  if(paren) outPut(")");
  action=cond->right;
  if(action==NULL){  // 2012-12-27 eat 0.8.13 - CID 751620
    outPut(";");
    return;
    }
  if(action->priority!=0) outPut("[%d]",action->priority);
  /* print the assertion list  */
  if(action->assert!=NULL){
    outPut(" ");
    printAssertions(action->assert);
    }
  if(action->command==NULL || !*action->command->value) outPut(";"); // 2012-12-27 eat 0.8.13 - CID 751568
  else{
    outPut(":");
    if(addrContext!=action->context){
      termPrintName(action->context);
      outPut(". ");
      }
    outPut("%s",action->command->value);
    }
  }
  
/*
*  Print conditions with a left and right operand
*/
static void condPrintInfix(struct COND *cond){
  outPut("(");
  printObject(cond->left);
  outPut("%s",cond->cell.object.type->name);   // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  printObject(cond->right); 
  outPut(")");
  }
  
static void condPrintDelay(struct COND *cond){
  outPut("(");
  printObject(cond->left);
  outPut("%s(",cond->cell.object.type->name);
  printObject(cond->right); 
  outPut("))");
  }
  
void condPrintTime(struct COND *cond){
  outPut("%s",cond->cell.object.type->name);   // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  outPut("(");
  schedPrint(cond->right);
  outPut(")");
  }

void condPrintPrefix(struct COND *cond){
  outPut("(");
  outPut("%s",cond->cell.object.type->name);  // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  printObject(cond->left);
  outPut(")");
  }
   
void condPrintMatch(struct COND *cond){
  outPut("(");
  printObject(cond->left);
  outPut("%s",cond->cell.object.type->name); // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  outPut("\"%s\"",((struct REGEXP *)cond->right)->string->value);
  outPut(")");
  }
  
void condPrintChange(struct COND *cond){
  outPut("(");
  outPut("%s",cond->cell.object.type->name); // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  printObject(cond->left);
  outPut(")");
  }
    
/*
*  Print selected conditions
*    0 - all conditions except rules
*    1 - relational conditions
*    2 - boolean conditions
*    3 - time conditions
*    4 - assertions
*/
void condPrintAll(int sel){
  NB_Type *type;
  
  for(type=nb_TypeList;type!=NULL;type=(NB_Type *)type->object.next){
    if((sel==0 && type->attributes&(TYPE_IS_RULE|TYPE_IS_REL|TYPE_IS_BOOL|TYPE_IS_TIME|TYPE_IS_ASSERT)) ||
       (sel==1 && type->attributes&TYPE_IS_REL) ||
       (sel==2 && type->attributes&TYPE_IS_BOOL) ||
       (sel==3 && type->attributes&TYPE_IS_TIME) ||
       (sel==4 && type->attributes&TYPE_IS_ASSERT)){
      nbHashShow(type->hash,type->name,NULL);
      }
    }
  }

/*************************************************************************
*  Condition Solve Methods
*/

/*
*  Solve generic prefix function (or any that only have left operand)
*/
void solvePrefix(struct COND *cond){
  nbCellSolve_(cond->left);
  return;
  }

/*
*  Solve generic infix condition that may only need one operand known
*/   
void solveInfix1(struct COND *cond){
  if(((NB_Object *)cond->left)->value==nb_Unknown){
    nbCellSolve_(cond->left);
    if(cond->cell.object.value!=nb_Unknown) return;
    } 
  if(((NB_Object *)cond->right)->value==nb_Unknown) nbCellSolve_(cond->right);   
  return;
  }

/*
*  Solve infix operation that needs both operands known
*/
void solveInfix2(struct COND *cond){
  if(((NB_Object *)cond->left)->value==nb_Unknown){
    if(nbCellSolve_(cond->left)==nb_Unknown) return;
    }
  if(((NB_Object *)cond->right)->value==nb_Unknown) nbCellSolve_(cond->right);   
  return; 
  }

void solveKnown(struct COND *cond){
  return;
  } 

/*************************************************************************
*  Condition Reaction Methods
*
*     Any value other than Unknown and False is "true" for Boolean operators
*     We should never encounter nb_Disabled operands except for disabled
*     conditions, and they should never get evaluated.
*/

/*
*  This function is used for rules as an alternative to nbCellAlert which schedules
*  cells for re-evaluation.  Rules are scheduled for action differently here.  However,
*  we still publish changes to subscribers.  We are able to assume the value has
*  changed, or we would not have been alerted. 
*/
void alertRule(NB_Cell *rule){
  struct ACTION *action;
  NB_Object *object=((NB_Object *)((struct COND *)rule)->left)->value;

  if(trace){
    outMsg(0,'T',"alertRule called %p",rule);
    printObject((NB_Object *)rule);
    outPut("\n");
    } 
  action=((struct COND *)rule)->right;         
  if(object==nb_Unknown || object==NB_OBJECT_FALSE){
    if(action->status=='S'){
      char name[1024];
      nbTermName(name,sizeof(name),action->term,locGloss);
      outMsg(0,'E',"Cycle error - %s condition untrue before firing",name);
      action->status='E';
      }
    rule->object.value=object;
    }
  else if(action->status!='R'){
    char name[1024];
    nbTermName(name,sizeof(name),action->term,locGloss);
    outMsg(0,'E',"Cycle error - %s repetitive firing suppressed - status=%c",name,action->status);
    action->status='E';
    rule->object.value=object;
    }
  // 2013-12-13 eat - commented out the check for NB_OBJECT_TRUE
  //else if(rule->object.value!=NB_OBJECT_TRUE){ // 2006/10/28 eat - make sure we didn't get alerted to a different true value
  else if(rule->object.value->type->attributes&TYPE_NOT_TRUE){ // 2013-12-13 eat - only schedule if old value was not true
    if(trace) outMsg(0,'T',"alertRule scheduled action %p",action);
    scheduleAction(action);
    //rule->object.value=NB_OBJECT_TRUE;
    //rule->object.value=object; // 2013-12-05 eat - pass the condition value thru
    }
  rule->object.value=object; // 2014-04-25 eat - always the condition value thru
  nbCellPublish(rule);
  if(trace) outMsg(0,'T',"alertRule returning");
  }

void alertRuleIf(NB_Cell *rule){
  NB_Action *action;
  NB_Object *object=((NB_Object *)((struct COND *)rule)->left)->value;

  if(trace){
    outMsg(0,'T',"alertRuleIf:  called %p",rule);
    printObject((NB_Object *)rule);
    outPut("\n");
    }
  action=((struct COND *)rule)->right;
  if(object==nb_Unknown || object==nb_False){
    if(action->cell.mode&NB_CELL_MODE_SCHEDULED){    // remove from scheduled IF rule list
      //outMsg(0,'T',"alertRuleIf: removing IF rule from node's true IF rule list");
      if(action->cell.object.next) ((NB_Action *)action->cell.object.next)->priorIf=action->priorIf;
      if(action->priorIf) action->priorIf->cell.object.next=action->cell.object.next;
      else ((NB_Node *)action->context->def)->ifrule=(NB_Action *)action->cell.object.next;
      action->cell.object.next=NULL;
      action->priorIf=NULL;
      action->cell.mode&=~NB_CELL_MODE_SCHEDULED;
      }
    }
  else if(!(action->cell.mode&NB_CELL_MODE_SCHEDULED)){  // schedule IF rule for response on any alert
    //outMsg(0,'T',"alertRuleIf: adding IF rule to node's true IF rule list");
    action->cell.object.next=(NB_Object *)((NB_Node *)action->context->def)->ifrule;
    if(action->cell.object.next) ((NB_Action *)action->cell.object.next)->priorIf=action;
    ((NB_Node *)action->context->def)->ifrule=action;
    action->priorIf=NULL;
    action->cell.mode|=NB_CELL_MODE_SCHEDULED;
    }
  rule->object.value=object; // 2013-12-05 eat - pass the condition value thru
  nbCellPublish(rule);
  if(trace) outMsg(0,'T',"alertRuleIf: returning");
  }


/*
*  This evaluation function converts all true values to True.  We may want to change
*  this to allow any value to pass through.  This would hold for the alertRule() function
*  as well.  For IF rules, we would need to change the test in contextAlert.
*
*  2013-09-29 eat - modified to pass all condition cell values through - revise the note above after testing
*/ 
NB_Object *evalRule(struct COND *cond){
  return((NB_Object *)((NB_Object *)cond->left)->value);
  //NB_Object *object=((NB_Object *)cond->left)->value;
  //if(object==nb_Unknown) return(nb_Unknown);
  //if(object==NB_OBJECT_FALSE)   return(NB_OBJECT_FALSE);
  //return(NB_OBJECT_TRUE);
  }

NB_Object *evalNerve(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  NB_String *name=cond->right;
  outMsgHdr(0,'I',"Nerve ");
  //printObject(name);
  outPut("%s",name->value);
  outPut("=");
  printObject(object);
  outPut("\n");
  return(object);
  }
   
/* Unknown: ?e converts Unknown to True and other values to False */
NB_Object *evalUnknown(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

/* Known: !?e  converts Known to True and Unknown to False */
NB_Object *evalKnown(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_FALSE);
  return(NB_OBJECT_TRUE);
  }

/* Assume False: -?e  converts Unknown to ! (False) */
NB_Object *evalAssumeFalse(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_FALSE);
  return(object);
  }

/* Assume True: +?e  converts Unknown to 1 (True) */
NB_Object *evalAssumeTrue(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_TRUE);
  return(object);
  }

/* Not: !e converts ! to 1 and all True values to ! */
NB_Object *evalNot(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(nb_Unknown);
  if(object==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

/* True: !!e converts all True values to 1 */
NB_Object *evalTrue(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown || object==NB_OBJECT_FALSE) return(object);
  return(NB_OBJECT_TRUE);
  }

NB_Object *evalDefault(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject==nb_Unknown) return(((NB_Object *)cond->right)->value);
  return(lobject);
  }

NB_Object *evalAndMonitor(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject==NB_OBJECT_FALSE || lobject==nb_Unknown){
    nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(nb_Unknown);
    }
  nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  return(((NB_Object *)cond->right)->value);
  }

NB_Object *evalLazyAnd(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE){
    nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(lobject);
    }
  nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown && robject!=NB_OBJECT_FALSE) return(lobject);
  return(robject);
  }

NB_Object *evalAnd(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE || robject==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE);
  if(lobject==nb_Unknown || robject==nb_Unknown) return(nb_Unknown);
  return(nb_True);
  }

NB_Object *reduceAnd(NB_Object *lobject,NB_Object *robject){
  if(lobject==NB_OBJECT_FALSE || robject==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE);
  if(lobject!=nb_Unknown && lobject->value==lobject) return(robject);
  if(lobject==nb_Unknown && robject->value==robject) return(nb_Unknown);
  return(NULL);
  }

NB_Object *evalNand(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE || robject==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  if(lobject==nb_Unknown || robject==nb_Unknown) return(nb_Unknown);
  return(NB_OBJECT_FALSE);
  }
  
NB_Object *evalOrMonitor(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject!=NB_OBJECT_FALSE){
    nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(nb_Unknown);
    }
  nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  return(((NB_Object *)cond->right)->value);
  }

NB_Object *evalLazyOr(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject!=NB_OBJECT_FALSE && lobject!=nb_Unknown){
    nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(lobject);
    }
  nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown && robject==NB_OBJECT_FALSE) return(lobject);
  return(robject);
  }

NB_Object *evalOr(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE){
    if(robject==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE);
    if(robject==nb_Unknown) return(nb_Unknown);
    return(nb_True);
    }
  if(lobject==nb_Unknown){
    if(robject==NB_OBJECT_FALSE || robject==nb_Unknown) return(nb_Unknown);
    return(nb_True);
    }
  return(nb_True);
  }

NB_Object *reduceOr(NB_Object *lobject,NB_Object *robject){
  if(lobject==NB_OBJECT_FALSE) return(robject);
  if(robject==NB_OBJECT_FALSE) return(lobject);
  if(lobject->value==lobject){    // left is constant and not false
    if(lobject!=nb_Unknown) return(lobject); 
    else if(robject->value==robject) return(robject);  // left is unknown and right is constant not true
    }
  // if left is variable we can't reduce because we don't know
  // which side would provide the True value---we can not assume it would be the right side
  return(NULL);
  }

NB_Object *evalNor(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE){
    if(robject==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
    if(robject==nb_Unknown) return(nb_Unknown);
    }
  else if(lobject==nb_Unknown){
    if(robject==NB_OBJECT_FALSE || robject==nb_Unknown) return(nb_Unknown);
    }
  return(NB_OBJECT_FALSE);
  }

NB_Object *evalXor(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown || robject==nb_Unknown) return(nb_Unknown);
  if(lobject==NB_OBJECT_TRUE && robject==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  if(lobject==NB_OBJECT_FALSE && robject==NB_OBJECT_TRUE) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }
    
NB_Object *evalAndCapture(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject==NB_OBJECT_FALSE || lobject==nb_Unknown) return(cond->cell.object.value);
  return(nbCellCompute_((NB_Cell *)cond->right));
  }

NB_Object *evalOrCapture(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject!=NB_OBJECT_FALSE) return(cond->cell.object.value);
  return(nbCellCompute_((NB_Cell *)cond->right));
  }

NB_Object *evalFlipFlop(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown || robject==nb_Unknown) return(cond->cell.object.value);
  if(lobject!=NB_OBJECT_FALSE && robject==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  if(lobject==NB_OBJECT_FALSE && robject!=NB_OBJECT_FALSE) return(NB_OBJECT_FALSE);
  return(cond->cell.object.value);
  }

// 2014-04-27 eat - alarm handler for state transition time  delays
static void alarmDelay(NB_Cond *cond){
  cond->cell.object.value=((NB_Object *)cond->left)->value;
  nbCellPublish((nbCELL)cond);
  cond->cell.mode&=~NB_CELL_MODE_TIMER;
  }

static NB_Object *evalDelayTrue(NB_Cond *cond){
  NB_Object *value=((NB_Object *)cond->left)->value;
  if(value==nb_False || value==nb_Unknown){
    if(cond->cell.mode&NB_CELL_MODE_TIMER){
      nbClockSetTimer(0,(NB_Cell *)cond);
      cond->cell.mode&=~NB_CELL_MODE_TIMER;
      }
    return(value);
    }
  nbClockSetTimer(nb_ClockTime+((NB_Sched *)cond->right)->duration,(NB_Cell *)cond);
  cond->cell.mode|=NB_CELL_MODE_TIMER;
  return(cond->cell.object.value);
  }

static NB_Object *evalDelayFalse(NB_Cond *cond){
  NB_Object *value=((NB_Object *)cond->left)->value;
  if(value!=nb_False){
    if(cond->cell.mode&NB_CELL_MODE_TIMER){
      nbClockSetTimer(0,(NB_Cell *)cond);
      cond->cell.mode&=~NB_CELL_MODE_TIMER;
      }
    return(value);
    }
  nbClockSetTimer(nb_ClockTime+((NB_Sched *)cond->right)->duration,(NB_Cell *)cond);
  cond->cell.mode|=NB_CELL_MODE_TIMER;
  return(cond->cell.object.value);
  }

static NB_Object *evalDelayUnknown(NB_Cond *cond){
  NB_Object *value=((NB_Object *)cond->left)->value;
  if(value!=nb_Unknown){
    if(cond->cell.mode&NB_CELL_MODE_TIMER){
      nbClockSetTimer(0,(NB_Cell *)cond);
      cond->cell.mode&=~NB_CELL_MODE_TIMER;
      }
    return(value);
    }
  nbClockSetTimer(nb_ClockTime+((NB_Sched *)cond->right)->duration,(NB_Cell *)cond);
  cond->cell.mode|=NB_CELL_MODE_TIMER;
  return(cond->cell.object.value);
  }

static NB_Object *evalTime(struct COND *cond){
  /* 1) Time condition reaction is caused by scheduled events */
  /*    because there are no time subordinate conditions */
  /* 2) Delay time conditions always transition to False, and are placed in */
  /*    True (delay) and nb_Disabled values by the parent "delay logic" condition. */  
  /* 3) Normal time conditions always invert from true to false and from !true to true */
  /*    scheduling from nb_Disabled to True is done at enable time */
  
  //outMsg(0,'T',"evalTime starting.");
  /* always return false for dalays */
  if(cond->cell.object.type!=condTypeTime) return(NB_OBJECT_FALSE);
  if(cond->cell.object.value==NB_OBJECT_TRUE){
    if(trace) outPut("evalTime calling condSchedule\n");
    condSchedule(cond,NB_OBJECT_TRUE); /* schedule the true event */
    outMsg(0,'T',"evalTime ending False.");  
    return(NB_OBJECT_FALSE);
    }
  condSchedule(cond,NB_OBJECT_FALSE); /* schedule the false value */
  //outMsg(0,'T',"evalTime ending True.");  
  return(NB_OBJECT_TRUE);
  }
    
static NB_Object *evalRelEQ(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left==right) return(NB_OBJECT_TRUE); 
  if(left->type==realType && right->type==realType &&
    ((struct REAL *)left)->value==((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalRelNE(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left==right) return(NB_OBJECT_FALSE);
  if(left->type==realType && right->type==realType &&
    ((struct REAL *)left)->value==((struct REAL *)right)->value) return(NB_OBJECT_FALSE);
  return(NB_OBJECT_TRUE);
  }
  
static NB_Object *evalRelLT(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left->type!=right->type) return(nb_Unknown);
  if(left->type==strType && 
    strcmp(((struct STRING *)left)->value,((struct STRING *)right)->value)<0) return(NB_OBJECT_TRUE);
  if(left->type==realType && 
    ((struct REAL *)left)->value<((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalRelLE(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left->type!=right->type) return(nb_Unknown);
  if(left->type==strType &&
    strcmp(((struct STRING *)left)->value,((struct STRING *)right)->value)<=0) return(NB_OBJECT_TRUE);
  if(left->type==realType &&
    ((struct REAL *)left)->value<=((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalRelGT(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left->type!=right->type) return(nb_Unknown);
  if(left->type==strType &&
    strcmp(((struct STRING *)left)->value,((struct STRING *)right)->value)>0) return(NB_OBJECT_TRUE);
  if(left->type==realType &&
    ((struct REAL *)left)->value>((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalRelGE(struct COND *cond){
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left->type!=right->type) return(nb_Unknown);
  if(left->type==strType &&
    strcmp(((struct STRING *)left)->value,((struct STRING *)right)->value)>=0) return(NB_OBJECT_TRUE);
  if(left->type==realType &&
    ((struct REAL *)left)->value>=((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalMatch(struct COND *cond){
  struct STRING *str=(struct STRING *)((NB_Term *)cond->left)->cell.object.value;
  struct REGEXP *regexp;
  int rc;

  if(trace) outMsg(0,'T',"evalMatch called.");
  if(str->object.value==nb_Unknown) return(nb_Unknown);
  if(str->object.type!=strType) return(NB_OBJECT_FALSE);
  regexp=cond->right;
  //if(regexec(&(regexp->regexp),str->value,(size_t) 0,NULL,0)==0) return(NB_OBJECT_TRUE);
  //outMsg(0,'T',"Calling pcre_exec: nsub=%d re=%s",regexp->nsub,regexp->string->value);
  rc=pcre_exec(regexp->re,regexp->pe,str->value,strlen(str->value),0,0,NULL,0);
  if(rc>=0){
    //outMsg(0,'T',"evalMatch: returning true");
    return(NB_OBJECT_TRUE);
    }
  if(rc!=PCRE_ERROR_NOMATCH) outMsg(0,'E',"evalMatch: pcre_exec rc=%d",rc);
  //outMsg(0,'T',"evalMatch: returning false");
  return(NB_OBJECT_FALSE);
  }

static NB_Object *evalChange(struct COND *cond){
  listInsert(&change,cond);
  return(NB_OBJECT_TRUE);
  }

void condChangeReset(void){
  NB_Link *list;
  struct COND *cond;
  for(list=change;list!=NULL;list=change){
    change=list->next; /* remove entry */
    list->next=nb_LinkFree;
    nb_LinkFree=list;
    cond=(struct COND *)(list->object);
    cond->cell.object.value=NB_OBJECT_FALSE;
    /* seems like we need to react here - but might it loop? */
    nbCellPublish((nbCELL)cond);
    }
  nbCellReact();
  }

// 2013-12-09 eat - Introducing axon cell for RelEQ evaluation acceleration
// RelEQ uses a axon cell when the right operand is a constant

static void enableRelEQ(struct COND *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonEnableRelEq((NB_Cell *)cond->left,cond);
      }
    return;
    } 
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

static void disableRelEQ(struct COND *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonDisableRelEq((NB_Cell *)cond->left,cond);
      }
    return;
    }
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

static void enableRelNE(NB_Cond *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonEnableRelNe((NB_Cell *)cond->left,cond);
      }
    return;
    }
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

static void disableRelNE(NB_Cond *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonDisableRelNe((NB_Cell *)cond->left,cond);
      }
    return;
    }
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

static void enableRelRange(NB_Cond *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonEnableRelRange((NB_Cell *)cond->left,cond);
      }
    return;
    }
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

static void disableRelRange(NB_Cond *cond){
  if(((NB_Cell*)cond->right)->object.value==cond->right){    // constant right
    if(((NB_Cell*)cond->left)->object.value!=cond->left){    // not constant left
      if(((NB_Cell*)cond->right)->object.value!=nb_Unknown)  // right not unknown, which would make cell value constant ?
        nbAxonDisableRelRange((NB_Cell *)cond->left,cond);
      }
    return;
    }
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

//
 
void enableRule(struct COND *cond){
  if(trace) outMsg(0,'T',"enableRule() called");
  if(cond->cell.object.value==nb_Disabled) {
    nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
    }
  }

void disableRule(struct COND *cond){
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void enablePrefix(struct COND *cond){
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void disablePrefix(struct COND *cond){
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void enableInfix(struct COND *cond){
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

void disableInfix(struct COND *cond){
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbAxonDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

void enableCapture(struct COND *cond){
  /* initialize capture to Unknown because evalAndCapture or evalOrCapture may return current value */
  cond->cell.object.value=nb_Unknown; 
  enablePrefix(cond);
  }
void enableFlipFlop(struct COND *cond){
  /* initialize flip-flop to Unknown because evalFlipFlop may return current value */
  cond->cell.object.value=nb_Unknown; 
  enableInfix(cond);
  }

void enableDelay(NB_Cond *cond){
  nbAxonEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }
void disableDelay(struct COND *cond){
  nbAxonDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }


/*
*  Enable time condition
*    By setting the value to true, we cause evalTime() to return False
*    and schedule the first True.
*/
void enableTime(struct COND *cond){
  cond->cell.object.value=NB_OBJECT_TRUE;
  }

void disableTime(struct COND *cond){
  /* cond->cell.object.value=NB_OBJECT_TRUE; */
  condUnschedule(cond);
  }

/**********************************************************************
*  Condition Destructors
*    Rules are special cases for now.  Eventually the rule condition
*    will be merged with the ACTION object.     
*/
void freeCondition(struct COND *cond){
  NB_Cond *lcond,**condP;
  NB_Hash *hash=cond->cell.object.type->hash;

  condP=(NB_Cond **)&(hash->vect[cond->cell.object.hashcode&hash->mask]);
  for(lcond=*condP;lcond!=NULL && lcond!=cond;lcond=*condP)
    condP=(struct COND **)&lcond->cell.object.next;
  if(lcond==cond) *condP=(struct COND *)cond->cell.object.next;
  cond->cell.object.next=(NB_Object *)condFree;
  condFree=cond;
  hash->objects--;
  }

void destroyCondition(struct COND *cond){
  nbAxonDisable(cond->left,(NB_Cell *)cond);  
  dropObject(cond->left);
  nbAxonDisable(cond->right,(NB_Cell *)cond);
  dropObject(cond->right);
  freeCondition(cond);
  }

void destroyNerve(struct COND *cond){
  nbAxonDisable(cond->left,(NB_Cell *)cond);  
  dropObject(cond->left);
  dropObject(cond->right);
  freeCondition(cond);
  }

void destroyRule(struct COND *cond){
  struct ACTION *action=cond->right;
  nbAxonDisable(cond->left,(NB_Cell *)cond);
  dropObject(cond->left);
  action->cond=NULL;            /* flag for deletion */
  if(action->status=='R'){
    action->nextAct=ashList;
    ashList=action;
    }
  action->status='D';   /* flag for deletion */
  freeCondition(cond);
  }	
 
/**********************************************************************
* Public Methods
**********************************************************************/
void nbConditionInit(NB_Stem *stem){
  condTypeNerve=newType(stem,"nerve",NULL,TYPE_RULE,condPrintNerve,destroyNerve);
  nbCellType(condTypeNerve,solvePrefix,evalNerve,enableRule,disableRule);

  condTypeOnRule=newType(stem,"on",NULL,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeOnRule,solvePrefix,evalRule,enableRule,disableRule);
  condTypeOnRule->alert=alertRule;
  condTypeWhenRule=newType(stem,"when",NULL,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeWhenRule,solvePrefix,evalRule,enableRule,disableRule);
  condTypeWhenRule->alert=alertRule;
  condTypeIfRule=newType(stem,"if",NULL,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeIfRule,solvePrefix,evalRule,enableRule,disableRule);
  condTypeIfRule->alert=alertRuleIf;

  condTypeNot=newType(stem,"!",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeNot,solvePrefix,evalNot,enablePrefix,disablePrefix);
  condTypeTrue=newType(stem,"!!",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeTrue,solvePrefix,evalTrue,enablePrefix,disablePrefix);
  condTypeUnknown=newType(stem,"?",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeUnknown,solvePrefix,evalUnknown,enablePrefix,disablePrefix);
  condTypeKnown=newType(stem,"!?",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeKnown,solvePrefix,evalKnown,enablePrefix,disablePrefix);
  condTypeAssumeFalse=newType(stem,"-?",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeAssumeFalse,solvePrefix,evalAssumeFalse,enablePrefix,disablePrefix);
  condTypeAssumeTrue=newType(stem,"+?",NULL,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeAssumeTrue,solvePrefix,evalAssumeTrue,enablePrefix,disablePrefix);

  condTypeDefault=newType(stem,"?",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeDefault,solveInfix1,evalDefault,enableInfix,disableInfix);
  condTypeLazyAnd=newType(stem,"&&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeLazyAnd,solveInfix1,evalLazyAnd,enablePrefix,disablePrefix); // prefix enabling is intentional
  condTypeAnd=newType(stem,"&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAnd,solveInfix1,evalAnd,enableInfix,disableInfix);
  condTypeNand=newType(stem,"!&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeNand,solveInfix1,evalNand,enableInfix,disableInfix);
  condTypeLazyOr=newType(stem,"||",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeLazyOr,solveInfix1,evalLazyOr,enablePrefix,disablePrefix);
  condTypeOr=newType(stem,"|",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOr,solveInfix1,evalOr,enableInfix,disableInfix);
  condTypeNor=newType(stem,"!|",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeNor,solveInfix1,evalNor,enableInfix,disableInfix);
  condTypeXor=newType(stem,"|!&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeXor,solveInfix1,evalXor,enableInfix,disableInfix);
  //condTypeAndMonitor=newType(stem,"&~&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  condTypeAndMonitor=newType(stem," then ",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAndMonitor,solveInfix2,evalAndMonitor,enableCapture,disableInfix);
  condTypeOrMonitor=newType(stem,"|~|",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOrMonitor,solveInfix2,evalOrMonitor,enableCapture,disableInfix);
  //condTypeAndCapture=newType(stem,"&^&",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  condTypeAndCapture=newType(stem," capture ",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAndCapture,solveInfix2,evalAndCapture,enableCapture,disableInfix);
  condTypeOrCapture=newType(stem,"|^|",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOrCapture,solveInfix2,evalOrCapture,enableCapture,disableInfix);
  condTypeFlipFlop=newType(stem,"^",NULL,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeFlipFlop,solveInfix2,evalFlipFlop,enableFlipFlop,disableInfix);

  condTypeDelayTrue=newType(stem,"~^",NULL,TYPE_DELAY,condPrintDelay,destroyCondition);
  nbCellType(condTypeDelayTrue,solveKnown,evalDelayTrue,enableDelay,disableDelay);
  condTypeDelayTrue->alarm=alarmDelay;
  condTypeDelayFalse=newType(stem,"~^!",NULL,TYPE_DELAY,condPrintDelay,destroyCondition);
  nbCellType(condTypeDelayFalse,solveKnown,evalDelayFalse,enableDelay,disableDelay);
  condTypeDelayFalse->alarm=alarmDelay;
  condTypeDelayUnknown=newType(stem,"~^?",NULL,TYPE_DELAY,condPrintDelay,destroyCondition);
  nbCellType(condTypeDelayUnknown,solveKnown,evalDelayUnknown,enableDelay,disableDelay);
  condTypeDelayUnknown->alarm=alarmDelay;

  condTypeTime=newType(stem,"~",NULL,TYPE_TIME,condPrintTime,destroyCondition);
  nbCellType(condTypeTime,solveKnown,evalTime,enableTime,disableTime);

  condTypeRelEQ=newType(stem,"=",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelEQ,solveInfix2,evalRelEQ,enableRelEQ,disableRelEQ);
  condTypeRelNE=newType(stem,"<>",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelNE,solveInfix2,evalRelNE,enableRelNE,disableRelNE);
  condTypeRelLT=newType(stem,"<",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelLT,solveInfix2,evalRelLT,enableRelRange,disableRelRange);
  condTypeRelLE=newType(stem,"<=",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelLE,solveInfix2,evalRelLE,enableRelRange,disableRelRange);
  condTypeRelGT=newType(stem,">",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelGT,solveInfix2,evalRelGT,enableRelRange,disableRelRange);
  condTypeRelGE=newType(stem,">=",NULL,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelGE,solveInfix2,evalRelGE,enableRelRange,disableRelRange);

  condTypeMatch=newType(stem,"~",NULL,0,condPrintMatch,destroyCondition);
  nbCellType(condTypeMatch,solvePrefix,evalMatch,enableInfix,disableInfix);
  condTypeChange=newType(stem,"~=",NULL,0,condPrintChange,destroyCondition);
  nbCellType(condTypeChange,solveKnown,evalChange,enableInfix,disableInfix);
  }

/*
*  This routine returns a pointer to a condition object.  If the
*  condition is already defined, the existing structure is returned.
*  Otherwise a new condition object is constructed.
*
*  Conditions are "disabled" until they are referenced by a rule.  Such
*  a reference is reported by nbAxonEnable which builds the back link list
*  to dependent conditions referenced by a rule.  This prevents the
*  eval routine from evaluating conditions not referenced by any
*  rule.
*
*/
struct COND * useCondition(struct TYPE *type,void *left,void *right){
  NB_Cond *cond,*loper,*roper,**condP;
  NB_Hash *hash=type->hash;
  uint32_t hashcode;
  unsigned long h,*l,*r;

  if(trace) outMsg(0,'T',"useCondition: called");
  l=(unsigned long *)&left;
  r=(unsigned long *)&right;
  h=((*l>>3)+(*r>>3));
  h+=(h>>3);
  hashcode=h;
  condP=(NB_Cond **)&(hash->vect[hashcode&hash->mask]);
  for(cond=*condP;cond!=NULL;cond=*condP){
    if(cond->left==left && cond->right==right) return(cond);
    condP=(struct COND **)&cond->cell.object.next;
    }
  cond=nbCellNew(type,(void **)&condFree,sizeof(struct COND));
  cond->cell.object.hashcode=hashcode;
  cond->cell.object.next=(NB_Object *)*condP;
  *condP=cond;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&type->hash);
  cond->left=grabObject(left);
  cond->right=grabObject(right);
  loper=left;
  roper=right;
  if(type->attributes&TYPE_IS_RULE){    /* rules */
    if(loper->cell.object.value!=(NB_Object *)loper){
      cond->cell.level=loper->cell.level+1;
      nbAxonEnable((NB_Cell *)left,(NB_Cell *)cond);
      left=loper->cell.object.value;
      }
    // 2013-09-29 - Set the value of a rule term to the value of the condition
    //if(left==nb_Unknown || left==NB_OBJECT_FALSE) cond->cell.object.value=left;
    //else cond->cell.object.value=NB_OBJECT_TRUE;
    cond->cell.object.value=loper->cell.object.value;
    }
  else{                     /* other conditions */
    if(loper->cell.object.value!=(NB_Object *)loper)
      cond->cell.level=loper->cell.level+1;
    if(roper->cell.object.value!=(NB_Object *)roper &&
      cond->cell.level<=roper->cell.level)
      cond->cell.level=roper->cell.level+1;
    cond->cell.object.value=nb_Disabled;
    }
  if(trace) outMsg(0,'T',"useCondition: returning");
  return(cond);
  }

/*
*  Schedule/Unschedule Time Condition Changes
*/
void condSchedule(struct COND *cond,NB_Object *value){
  if(trace){
    outMsg(0,'T',"condSchedule:");
    printObject((NB_Object *)cond);
    outPut("\n");
    outFlush();
    }
  if(!(cond->cell.object.type->attributes&TYPE_IS_TIME)){
    outMsg(0,'L',"condSchedule: cond must be time condition.");
    return;
    }
  if(cond->cell.object.type==condTypeTime){ /* not a delay */
    if(value==NB_OBJECT_TRUE) nbClockSetTimer(schedNext(nb_ClockTime,cond->right),(NB_Cell *)cond);
    else nbClockSetTimer(schedNext(0,cond->right),(NB_Cell *)cond);
    }  
  else if(value==NB_OBJECT_FALSE) nbClockSetTimer(schedNext(nb_ClockTime,cond->right),(NB_Cell *)cond);
  else {
    outMsg(0,'L',"condSchedule: scheduled value for delay timer must be false.");
    outPut("object:");
    printObject((NB_Object *)cond);
    outPut("\nvalue:");
    printObject(value);
    outFlush();
    }
  if(trace) outMsg(0,'T',"condSchedule complete.");
  }
  
void condUnschedule(struct COND *cond){
  /* This layer can probably be removed after the event routines are stable */
  if(!(cond->cell.object.type->attributes&TYPE_IS_TIME)){
    outMsg(0,'L',"condUnschedule: cond must be time condition.");
    return;
    }
  nbClockSetTimer(0,(NB_Cell *)cond);      /* unschedule value change */
  }
