/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbcondition.h 
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
*=============================================================================
*/
#include <nb/nbi.h>

struct ACTION *actList;  /* action list (proactive or reactive THEN) */
struct ACTION *ashList;  /* list of rules that fired */

struct COND *condFree=NULL;
struct HASH *condH;

struct TYPE *condTypeNerve;    /* nerve() - nerve cells simply report new values */
struct TYPE *condTypeOnRule;   /* on()    - on(cond,action) - asserts and alerts */
struct TYPE *condTypeWhenRule; /* when()  - when(cond,action) - one time on() rule */
struct TYPE *condTypeIfRule;   /* if()    - if(cond,action) - alerts */
struct TYPE *condTypeNot;      /* !c      - not(cond) */
struct TYPE *condTypeTrue;     /* !!c     - true(cond) - All true values compression to 1 */
struct TYPE *condTypeUnknown;  /* ?c      - unknown(cond) */
struct TYPE *condTypeKnown;    /* !?c     - known(cond) */
struct TYPE *condTypeClosedWorld;  /* []c - closedworld(cond) - ? converts to 0*/
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
struct TYPE *condTypeTimeDelay;    /* ~x()   - timespec for time delays */
struct TYPE *condTypeDelayTrue;    /* ~^1() ~T()   - logic part of delay(cond,timespec)*/
struct TYPE *condTypeDelayFalse;   /* ~^0() ~F()   - logic part of delay(cond,timespec)*/
struct TYPE *condTypeDelayUnknown; /* ~^?() ~U()   - logic part of delay(cond,timespec)*/
struct TYPE *condTypeMatch;        /* a~"..."  - match(term,regexp) */
struct TYPE *condTypeChange;       /* a~~      - change(term) */

/*struct TYPE *condTypeCache; */        /* c(<list>)- exists(cache,list) */

void condSchedule();
void condUnschedule();

/**********************************************************************
* Support Routines
*/
/*
*  Hash a condition and return a pointer to a pointer in the hashing vector.
*/
void *hashCond(struct HASH *hash,struct TYPE *type,void *left,void *right){
  long h,*t,*l,*r;
  //printf("hashCond: sizeof(long)=%d sizeof(void *)=%d\n",sizeof(h),sizeof(void *));
  t=(long *)&type;
  l=(long *)&left;
  r=(long *)&right;
  h=(*l & 0x0fffffff) + (*r & 0x0fffffff) + (*t & 0x0fffffff);
  //printf("hashCond: n=%d modulo=%d index=%d\n",h,hash->modulo,h%hash->modulo);
  return(&(hash->vect[h%hash->modulo]));
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
    ((NB_Object *)cond->left)->type==condTypeNode ||
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
void condPrintInfix(cond) struct COND *cond; {
  outPut("(");
  printObject(cond->left);
  outPut("%s",cond->cell.object.type->name);   // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  printObject(cond->right); 
  outPut(")");
  }
  
void condPrintTime(cond) struct COND *cond; {
  outPut("%s",cond->cell.object.type->name);   // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  outPut("(");
  schedPrint(cond->right);
  outPut(")");
  }

void condPrintPrefix(cond) struct COND *cond; {
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
  
void condPrintChange(cond) struct COND *cond; {
  outPut("(");
  outPut("%s",cond->cell.object.type->name); // 2012-12-31 eat - VID 5328-0.8.13-1 added format string
  printObject(cond->left);
  outPut(")");
  }
    
void condPrintAll(int sel){
  /*
  *  Print selected conditions
  *    0 - all conditions except rules
  *    1 - relational conditions 
  *    2 - boolean conditions
  *    3 - time conditions
  */
  struct COND *cond,**condP;
  long v;
  long i;
  condP=(struct COND **)&(condH->vect);
  for(v=0;v<condH->modulo;v++){
    i=0;
    for(cond=*condP;cond!=NULL;cond=(struct COND *)cond->cell.object.next){
      if((sel==0 && !(cond->cell.object.type->attributes&TYPE_IS_RULE)) ||
        (sel==1 && cond->cell.object.type->attributes&TYPE_IS_REL) ||
        (sel==2 && cond->cell.object.type->attributes&TYPE_IS_BOOL) ||
        (sel==3 && cond->cell.object.type->attributes&TYPE_IS_TIME)) {
        outPut("H[%u,%ld]",v,i);
        outPut("R[%u]",cond->cell.object.refcnt); 
        outPut("L(%d)",cond->cell.level);
        outPut(" = ");
        printObject(cond->cell.object.value);
        outPut(" == ");
        printObject((NB_Object *)cond);
        outPut("\n");
        }
      if(i<LONG_MAX) i++;  // 2013-01-13 eat - VID 7045
      }
    condP++; 
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
    outMsg(0,'T',"alertRule called %u",rule);
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
  else if(rule->object.value!=NB_OBJECT_TRUE){ // 2006/10/28 eat - make sure we didn't get alerted to a different true value
    if(trace) outMsg(0,'T',"alertRule scheduled action %u",action);
    scheduleAction(action);
    rule->object.value=NB_OBJECT_TRUE;
    }
  nbCellPublish(rule);
  }

/*
*  This evaluation function converts all true values to True.  We may want to change
*  this to allow any value to pass through.  This would hold for the alertRule() function
*  as well.  For IF rules, we would need to change the test in contextAlert.
*/ 
NB_Object *evalRule(struct COND *cond){
  NB_Object *object=((NB_Object *)cond->left)->value;
  //outMsg(0,'T',"evalRule() called\n");
  if(object==nb_Unknown) return(nb_Unknown);
  if(object==NB_OBJECT_FALSE)   return(NB_OBJECT_FALSE);
  return(NB_OBJECT_TRUE);
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
NB_Object *evalKnown(cond) struct COND *cond; {
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_FALSE);
  return(NB_OBJECT_TRUE);
  }

/* Closed Word Value: [e] converts Unknown to 0 (False) */
NB_Object *evalClosedWorld(cond) struct COND *cond; {
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(NB_OBJECT_FALSE);
  return(object);
  }

/* Not: !e converts 0 to 1 and all True values to 0 */
NB_Object *evalNot(cond) struct COND *cond; {
  NB_Object *object=((NB_Object *)cond->left)->value;
  if(object==nb_Unknown) return(nb_Unknown);
  if(object==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

/* True: !!e converts all True values to 1 */
NB_Object *evalTrue(cond) struct COND *cond; {
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
    nbCellDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(nb_Unknown);
    }
  nbCellEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  return(((NB_Object *)cond->right)->value);
  }

NB_Object *evalLazyAnd(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE){
    nbCellDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(lobject);
    }
  nbCellEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown && robject!=NB_OBJECT_FALSE) return(lobject);
  return(robject);
  }

NB_Object *evalAnd(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE || (lobject==nb_Unknown && robject!=NB_OBJECT_FALSE)) return(lobject);
  return(robject);
  }

NB_Object *reduceAnd(NB_Object *lobject,NB_Object *robject){
  if(lobject==NB_OBJECT_FALSE || robject==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE);
  if(lobject!=nb_Unknown && lobject->value==lobject) return(robject);
  if(lobject==nb_Unknown && robject->value==robject) return(nb_Unknown);
  return(NULL);
  }

NB_Object *evalNand(cond) struct COND *cond; {
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE || robject==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
  if(lobject==nb_Unknown || robject==nb_Unknown) return(nb_Unknown);
  return(NB_OBJECT_FALSE);
  }
  
NB_Object *evalOrMonitor(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  if(lobject!=NB_OBJECT_FALSE){
    nbCellDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(nb_Unknown);
    }
  nbCellEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  return(((NB_Object *)cond->right)->value);
  }

NB_Object *evalLazyOr(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject!=NB_OBJECT_FALSE && lobject!=nb_Unknown){
    nbCellDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
    return(lobject);
    }
  nbCellEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  robject=((NB_Object *)cond->right)->value;
  if(lobject==nb_Unknown && robject==NB_OBJECT_FALSE) return(lobject);
  return(robject);
  }

NB_Object *evalOr(struct COND *cond){
  NB_Object *lobject=((NB_Object *)cond->left)->value;
  NB_Object *robject=((NB_Object *)cond->right)->value;
  if(lobject==NB_OBJECT_FALSE || (lobject==nb_Unknown && robject!=NB_OBJECT_FALSE)) return(robject);
  return(lobject);
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

NB_Object *evalNor(cond) struct COND *cond; {
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

NB_Object *evalXor(cond) struct COND *cond; {
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

NB_Object *evalDelay(cond) struct COND *cond; {
  /* delay reaction is caused by a scheduled event or subordinate value change */
  struct COND *lcond=cond->left,*rcond=cond->right;
  NB_Object *value=lcond->cell.object.value;
  if(trace) outMsg(0,'T',"evalDelay starting.");
  if((cond->cell.object.type==condTypeDelayTrue && (value==NB_OBJECT_FALSE || value==nb_Unknown)) ||
     (cond->cell.object.type==condTypeDelayFalse && value!=NB_OBJECT_FALSE) ||
     (cond->cell.object.type==condTypeDelayUnknown && value!=nb_Unknown)){
      if(rcond->cell.object.value==NB_OBJECT_TRUE) condUnschedule(rcond);  /* unschedule timer */
      rcond->cell.object.value=nb_Disabled;       /* set time value to disabled */
      return(value);                               /* and pass left value through */
      }
  else if(rcond->cell.object.value==NB_OBJECT_FALSE){    /* delay timer expired */
    return(value);                                 /* pass delayed value */ 
    }    
  else if(rcond->cell.object.value==nb_Disabled || rcond->cell.object.value==nb_Unknown){
    condSchedule(rcond,NB_OBJECT_FALSE);     /* need to schedule a delay */
    rcond->cell.object.value=NB_OBJECT_TRUE;             /* set timer value to true */
    return(cond->cell.object.value);           /* retain "remembered" value */
    }
  outMsg(0,'L',"evalDelay unexpected %d state on timer.",rcond->cell.object.value);
  return(nb_Unknown);  /* is this a good thing to return? */  
  }
  
NB_Object *evalTime(cond) struct COND *cond; {
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
    //outMsg(0,'T',"evalTime ending False.");  
    return(NB_OBJECT_FALSE);
    }
  condSchedule(cond,NB_OBJECT_FALSE); /* schedule the false value */
  //outMsg(0,'T',"evalTime ending True.");  
  return(NB_OBJECT_TRUE);
  }
    
NB_Object *evalRelEQ(cond) struct COND *cond; {
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left==right) return(NB_OBJECT_TRUE); 
  if(left->type==realType && right->type==realType &&
    ((struct REAL *)left)->value==((struct REAL *)right)->value) return(NB_OBJECT_TRUE);
  return(NB_OBJECT_FALSE);
  }

NB_Object *evalRelNE(cond) struct COND *cond; {
  NB_Object *left=((NB_Object *)cond->left)->value;
  NB_Object *right=((NB_Object *)cond->right)->value;
  if(left==nb_Unknown || right==nb_Unknown) return(nb_Unknown);
  if(left==right) return(NB_OBJECT_FALSE);
  if(left->type==realType && right->type==realType &&
    ((struct REAL *)left)->value==((struct REAL *)right)->value) return(NB_OBJECT_FALSE);
  return(NB_OBJECT_TRUE);
  }
  
NB_Object *evalRelLT(cond) struct COND *cond;{
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

NB_Object *evalRelLE(cond) struct COND *cond;{
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

NB_Object *evalRelGT(cond) struct COND *cond;{
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

NB_Object *evalRelGE(cond) struct COND *cond;{
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

NB_Object *evalMatch(struct COND *cond){
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

NB_Object *evalChange(struct COND *cond){
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
    /* seems like we need to purculate here - but might it loop? */
    nbCellPublish((nbCELL)cond);
    }
  nbCellReact();
  }
 
void enableRule(cond) struct COND *cond;{
  if(trace) outMsg(0,'T',"enableRule() called");
  if(cond->cell.object.value==nb_Disabled) {
    nbCellEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
    }
  }

void disableRule(cond) struct COND *cond;{
  nbCellDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void enablePrefix(cond) struct COND *cond; {
  nbCellEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void disablePrefix(cond) struct COND *cond; {
  nbCellDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  }

void enableInfix(cond) struct COND *cond; {
  nbCellEnable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbCellEnable((NB_Cell *)cond->right,(NB_Cell *)cond);
  }

void disableInfix(struct COND *cond){
  nbCellDisable((NB_Cell *)cond->left,(NB_Cell *)cond);
  if(cond->right!=cond->left) nbCellDisable((NB_Cell *)cond->right,(NB_Cell *)cond);
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
  struct COND *lcond,**condP;
  
  condP=hashCond(condH,cond->cell.object.type,cond->left,cond->right);
  if(*condP==cond) *condP=(struct COND *)cond->cell.object.next;
  else{
    for(lcond=*condP;lcond!=NULL && lcond!=cond;lcond=*condP)
      condP=(struct COND **)&lcond->cell.object.next;
    if(lcond==cond) *condP=(struct COND *)cond->cell.object.next;  
    }
  cond->cell.object.next=(NB_Object *)condFree;
  condFree=cond;
  } 

void destroyCondition(struct COND *cond){
  nbCellDisable(cond->left,(NB_Cell *)cond);  
  dropObject(cond->left);
  nbCellDisable(cond->right,(NB_Cell *)cond);
  dropObject(cond->right);
  freeCondition(cond);
  }

void destroyNerve(struct COND *cond){
  nbCellDisable(cond->left,(NB_Cell *)cond);  
  dropObject(cond->left);
  dropObject(cond->right);
  freeCondition(cond);
  }

void destroyRule(struct COND *cond){
  struct ACTION *action=cond->right;
  nbCellDisable(cond->left,(NB_Cell *)cond);
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
void initCondition(NB_Stem *stem){
  condH=newHash(100003);  /* initialize condition hash */
  condTypeNerve=newType(stem,"nerve",condH,TYPE_RULE,condPrintNerve,destroyNerve);
  nbCellType(condTypeNerve,solvePrefix,evalNerve,enableRule,disableRule);

  condTypeOnRule=newType(stem,"on",condH,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeOnRule,solvePrefix,evalRule,enableRule,disableRule);
  condTypeOnRule->alert=alertRule;
  condTypeWhenRule=newType(stem,"when",condH,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeWhenRule,solvePrefix,evalRule,enableRule,disableRule);
  condTypeWhenRule->alert=alertRule;
  condTypeIfRule=newType(stem,"if",condH,TYPE_RULE,condPrintRule,destroyRule);
  nbCellType(condTypeIfRule,solvePrefix,evalRule,enableRule,disableRule);

  condTypeNot=newType(stem,"!",condH,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeNot,solvePrefix,evalNot,enablePrefix,disablePrefix);
  condTypeTrue=newType(stem,"!!",condH,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeTrue,solvePrefix,evalTrue,enablePrefix,disablePrefix);
  condTypeUnknown=newType(stem,"?",condH,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeUnknown,solvePrefix,evalUnknown,enablePrefix,disablePrefix);
  condTypeKnown=newType(stem,"!?",condH,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeKnown,solvePrefix,evalKnown,enablePrefix,disablePrefix);
  condTypeClosedWorld=newType(stem,"[]",condH,TYPE_BOOL,condPrintPrefix,destroyCondition);
  nbCellType(condTypeClosedWorld,solvePrefix,evalClosedWorld,enablePrefix,disablePrefix);

  condTypeDefault=newType(stem,"?",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeDefault,solveInfix1,evalDefault,enableInfix,disableInfix);
  condTypeLazyAnd=newType(stem,"&&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeLazyAnd,solveInfix1,evalLazyAnd,enablePrefix,disablePrefix); // prefix enabling is intentional
  condTypeAnd=newType(stem,"&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAnd,solveInfix1,evalAnd,enableInfix,disableInfix);
  condTypeNand=newType(stem,"!&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeNand,solveInfix1,evalNand,enableInfix,disableInfix);
  condTypeLazyOr=newType(stem,"||",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeLazyOr,solveInfix1,evalLazyOr,enablePrefix,disablePrefix);
  condTypeOr=newType(stem,"|",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOr,solveInfix1,evalOr,enableInfix,disableInfix);
  condTypeNor=newType(stem,"!|",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeNor,solveInfix1,evalNor,enableInfix,disableInfix);
  condTypeXor=newType(stem,"|!&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeXor,solveInfix1,evalXor,enableInfix,disableInfix);
  condTypeAndMonitor=newType(stem,"&~&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAndMonitor,solveInfix2,evalAndMonitor,enableCapture,disableInfix);
  condTypeOrMonitor=newType(stem,"|~|",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOrMonitor,solveInfix2,evalOrMonitor,enableCapture,disableInfix);
  condTypeAndCapture=newType(stem,"&^&",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeAndCapture,solveInfix2,evalAndCapture,enableCapture,disableInfix);
  condTypeOrCapture=newType(stem,"|^|",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeOrCapture,solveInfix2,evalOrCapture,enableCapture,disableInfix);
  condTypeFlipFlop=newType(stem,"^",condH,TYPE_BOOL,condPrintInfix,destroyCondition);
  nbCellType(condTypeFlipFlop,solveInfix2,evalFlipFlop,enableFlipFlop,disableInfix);

  condTypeDelayTrue=newType(stem,"~^1",condH,TYPE_DELAY,condPrintInfix,destroyCondition);
  nbCellType(condTypeDelayTrue,solveKnown,evalDelay,enableInfix,disableInfix);
  condTypeDelayFalse=newType(stem,"~^0",condH,TYPE_DELAY,condPrintInfix,destroyCondition);
  nbCellType(condTypeDelayFalse,solveKnown,evalDelay,enableInfix,disableInfix);
  condTypeDelayUnknown=newType(stem,"~^?",condH,TYPE_DELAY,condPrintInfix,destroyCondition);
  nbCellType(condTypeDelayUnknown,solveKnown,evalDelay,enableInfix,disableInfix);

  condTypeTime=newType(stem,"~",condH,TYPE_TIME,condPrintTime,destroyCondition);
  nbCellType(condTypeTime,solveKnown,evalTime,enableTime,disableTime);
  condTypeTimeDelay=newType(stem,"",condH,TYPE_TIME,condPrintTime,destroyCondition);
  nbCellType(condTypeTimeDelay,solveKnown,evalTime,enableTime,disableTime);

  condTypeRelEQ=newType(stem,"=",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelEQ,solveInfix2,evalRelEQ,enableInfix,disableInfix);
  condTypeRelNE=newType(stem,"<>",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelNE,solveInfix2,evalRelNE,enableInfix,disableInfix);
  condTypeRelLT=newType(stem,"<",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelLT,solveInfix2,evalRelLT,enableInfix,disableInfix);
  condTypeRelLE=newType(stem,"<=",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelLE,solveInfix2,evalRelLE,enableInfix,disableInfix);
  condTypeRelGT=newType(stem,">",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelGT,solveInfix2,evalRelGT,enableInfix,disableInfix);
  condTypeRelGE=newType(stem,">=",condH,TYPE_REL,condPrintInfix,destroyCondition);
  nbCellType(condTypeRelGE,solveInfix2,evalRelGE,enableInfix,disableInfix);

  condTypeMatch=newType(stem,"~",condH,0,condPrintMatch,destroyCondition);
  nbCellType(condTypeMatch,solvePrefix,evalMatch,enableInfix,disableInfix);
  condTypeChange=newType(stem,"~=",condH,0,condPrintChange,destroyCondition);
  nbCellType(condTypeChange,solveKnown,evalChange,enableInfix,disableInfix);

  }

struct COND * useCondition(int not,struct TYPE *type,void *left,void *right){
  /*
  *  This routine returns a pointer to a condition object.  If the
  *  condition is already defined, the existing structure is returned.
  *  Otherwise a new condition object is constructed. 
  *
  *  Conditions are "disabled" until they are referenced by a rule.  Such
  *  a reference is reported by nbCellEnable which builds the back link list
  *  to dependent conditions referenced by a rule.  This prevents the
  *  eval routine from evaluating conditions not referenced by any
  *  rule.
  *
  */
  struct COND *cond,*lcond,**condP;
  if(trace) outMsg(0,'T',"useCondition() called");
  if(not){
    lcond=useCondition(0,type,left,right);
    return(useCondition(0,condTypeNot,lcond,nb_Unknown));
    }
  /* create a subordinate time condition unique to left condition */  
  if(type==condTypeDelayTrue || type==condTypeDelayFalse || type==condTypeDelayUnknown)
    right=useCondition(0,condTypeTimeDelay,left,right);
  condP=hashCond(condH,type,left,right);
  for(cond=*condP;cond!=NULL;cond=*condP){
    if(cond->left==left && cond->right==right && cond->cell.object.type==type) return(cond);
    condP=(struct COND **)&cond->cell.object.next;  
    }
  cond=nbCellNew(type,(void **)&condFree,sizeof(struct COND));
  cond->cell.object.next=(NB_Object *)*condP;
  *condP=cond;
  cond->left=grabObject(left);
  cond->right=grabObject(right);
  lcond=left;
  if(type->attributes&TYPE_IS_RULE){    /* rules */
    if(lcond->cell.object.value!=(NB_Object *)lcond){
      cond->cell.level=lcond->cell.level+1;
      nbCellEnable((NB_Cell *)lcond,(NB_Cell *)cond);
      left=lcond->cell.object.value;
      }
    if(left==nb_Unknown || left==NB_OBJECT_FALSE) cond->cell.object.value=left;
    else cond->cell.object.value=NB_OBJECT_TRUE;

/*  2002/08/19 eat - set rule value to condition value
    cond->cell.object.value=nb_Unknown;
*/
    }
  else{                     /* other conditions */
    if(lcond->cell.object.value!=(NB_Object *)lcond)
      cond->cell.level=lcond->cell.level+1;
    lcond=right;
    if(lcond->cell.object.value!=(NB_Object *)lcond &&
      cond->cell.level<=lcond->cell.level)
      cond->cell.level=lcond->cell.level+1;
    cond->cell.object.value=nb_Disabled;
    }
  if(trace) outMsg(0,'T',"useCondition() returning");
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