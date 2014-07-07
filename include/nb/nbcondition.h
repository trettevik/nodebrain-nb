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
* Title:    Condition Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain CONDITION objects.  A
*   nodebrain CONDITION is an extension of NB_CELL.  
*
* See nbcondition.c for more information.
*============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ----------------------------------------------------------------
* 2002-09-07 Ed Trettevik (split out in 0.4.1)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2014-01-12 eat 0.9.00 Eliminated hash pointer - access via types
* 2014-01-12 eat 0.9.00 Modified hash to return key instead of hash vector entry
*============================================================================
*/
#ifndef _NB_CONDITION_H_
#define _NB_CONDITION_H_

typedef struct COND {               /* Condition Object - one or two operands */
  /* This object is pointed to by the TERMs and the left and right
  *  fields of COND.  The operands may also be other object types,
  *  including TERM.
  */
  struct NB_CELL cell;     /* cell header */
  void   *left;            /* Left Operand     */
  void   *right;           /* Right Operand    */
  } NB_Cond;

extern struct COND *condFree;

extern struct TYPE *condTypeNerve;
extern struct TYPE *condTypeOnRule;
extern struct TYPE *condTypeWhenRule;
extern struct TYPE *condTypeIfRule;
extern struct TYPE *condTypeNot;
extern struct TYPE *condTypeTrue;
extern struct TYPE *condTypeUnknown;
extern struct TYPE *condTypeKnown;
extern struct TYPE *condTypeAssumeFalse;
extern struct TYPE *condTypeAssumeTrue;
extern struct TYPE *condTypeDefault;
extern struct TYPE *condTypeLazyAnd;
extern struct TYPE *condTypeAnd;
extern struct TYPE *condTypeNand;
extern struct TYPE *condTypeLazyOr;
extern struct TYPE *condTypeOr;
extern struct TYPE *condTypeNor;
extern struct TYPE *condTypeXor;
extern struct TYPE *condTypeAndMonitor;
extern struct TYPE *condTypeOrMonitor;
extern struct TYPE *condTypeAndCapture;
extern struct TYPE *condTypeOrCapture;
extern struct TYPE *condTypeFlipFlop;
extern struct TYPE *condTypeRelEQ;
extern struct TYPE *condTypeRelNE;
extern struct TYPE *condTypeRelLT;
extern struct TYPE *condTypeRelLE;
extern struct TYPE *condTypeRelGT;
extern struct TYPE *condTypeRelGE;
extern struct TYPE *condTypeTime;
extern struct TYPE *condTypeTimeDelay;
extern struct TYPE *condTypeDelayTrue;
extern struct TYPE *condTypeDelayFalse;
extern struct TYPE *condTypeDelayUnknown;
extern struct TYPE *condTypeMatch;
extern struct TYPE *condTypeChange;

/*
*  Public methods
*/
void nbConditionInit(NB_Stem *stem);
struct COND *useCondition(struct TYPE *type,void *left,void *right);
//void *hashCond(struct HASH *hash,struct TYPE *type,void *left,void *right);
uint32_t hashCond(struct TYPE *type,void *left,void *right);
void destroyCondition(struct COND *cond);
void freeCondition(struct COND *cond);

void condSchedule(struct COND *cond,NB_Object *value);
void condUnschedule(struct COND *cond);

void condPrintAll(int sel);

void condChangeReset(void);

// This will probably become a cell method - if so we will not need to define here
NB_Object *reduceAnd(NB_Object *lobject,NB_Object *robject);
NB_Object *reduceOr(NB_Object *lobject,NB_Object *robject);

#endif
