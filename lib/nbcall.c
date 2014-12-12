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
* File:     nbcall.c 
*
* Title:    Call Functions 
*
* Function:
*
*   This file provides methods for NodeBrain CALL objects.  The CALL type
*   extends COND currently
*
* Synopsis:
*
*   #include "nb.h"
*
*   void NbCallInit();
*
*   NB_Call *NbCallUse(nbCELL context,char *ident,NB_List *list);
* 
* Description
*
*   The CALL object type provides an easy way to extend nodebrain
*   functionality.  Functions can be included in this file to support any
*   number of operations on object lists.  The list members may be any
*   cell expression as illustrated by the following example.
*
*      `math.mod(3*a,b/2)
*
*   The NbCallInit() method has the responsibility of creating an individual
*   object type for each such function.  The "eval" method for the type
*   must perform the desired operation on the list members to produce and
*   publish a new value each time a new value is published for a list
*   member.
*   
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-07 Ed Trettevik (original version introduced in 0.4.1)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning message. (gcc 4.5.0)
* 2013-01-13 eat 0.8.13 Checker updates - rosechecker
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
* 2014-11-23 eat 0.9.03 Added functions for declaring cell bindings and functions
*=============================================================================
*/
#define _XOPEN_SOURCE 600   // 2014-12-06 eat - for trunc and round

#include <nb/nbi.h>
#include <limits.h>

struct TYPE *callType=NULL;
struct TYPE *callTypeMod=NULL;
struct TYPE *callTypeTrace=NULL;

typedef struct NB_CALL_BINDING{
 NB_Object  object;
 NB_String *name;
 NB_Object *(*binder)();
 int        argc;  // number of arguments to check at parse time, -1 if variadic
 } NB_CallBinding;

NB_CallBinding *callBindings=NULL;     // list of bindings

typedef struct NB_CALL_FUNCTION{
  NB_Object  object;
  NB_String *name;
  void      *function;
  NB_CallBinding *binding;
  } NB_CellFunction;

NB_CellFunction *cellFunctions=NULL;  // list of functions - may change to hash

/* 
* NOTE: 
*
*   Effectively, we are extending COND with call by adding more types
*   and associated evaluation methods.  This structure must have the
*   same basic structure as COND (cell and two pointers).
*/
typedef struct CALL{                /* Call cell - one or two operands */
  struct NB_CELL   cell;     /* cell header */
  NB_CellFunction *function; // call function
  struct NB_LIST   *list;    /* parameter list */
  } NB_Call;

/**********************************************************************
* Private Object Methods
**********************************************************************/
static void printCall(call) struct CALL *call;{
  if(call==NULL) outPut("(?)");
  else{
    outPut("`%s",call->cell.object.type->name);
    printObject((NB_Object *)call->list);
    }
  }

static void printCall2(NB_Call *call){
  if(call==NULL) outPut("(?)");
  else{
    outPut("`%s",call->function->name->value);
    printObject((NB_Object *)call->list);
    }
  }

/*
*  Call destructor  - using destroyCondition()
*/

/**********************************************************************
* Private Function Calculation Methods
**********************************************************************/

static NB_Object *bindCallD_DD(double (*function)(),NB_List *list){
  NB_Link *member;
  struct REAL *lreal,*rreal;
  double value;

  if(list==NULL) return(nb_Unknown);
  if((member=list->link)==NULL) return(nb_Unknown);
  if((lreal=(struct REAL *)member->object)==NULL) return(nb_Unknown);
  if((member=member->next)==NULL) return(nb_Unknown);
  if((rreal=(struct REAL *)member->object)==NULL) return(nb_Unknown);
  lreal=(struct REAL *)((NB_Object *)lreal)->value;
  rreal=(struct REAL *)((NB_Object *)rreal)->value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  value=(*function)(lreal->value,rreal->value);
  // maybe detect NaN and infinity values and return nb_Unknown here
  return((NB_Object *)useReal(value));
  }

static NB_Object *bindCallD_D(double (*function)(),NB_List *list){
  NB_Link *member;
  NB_Object *object;
  struct REAL *real;
  double value;

  if(list==NULL) return(nb_Unknown);
  if((member=list->link)==NULL) return(nb_Unknown);
  if((object=member->object)==NULL) return(nb_Unknown);
  real=(NB_Real *)object->value;
  if(real->object.type!=realType) return(nb_Unknown);
  value=(*function)(real->value);
  // maybe detect NaN and infinity values and return nb_Unknown here
  return((NB_Object *)useReal(value));
  }

static NB_Object *bindCallS_SI(char *(*function)(),NB_List *list){
  NB_Link *member;
  NB_Real *real;
  NB_String *string;
  NB_Object *object1,*object2;
  int i;
  char *value;

  if(list==NULL) return(nb_Unknown);
  if((member=list->link)==NULL) return(nb_Unknown);
  if((object1=member->object)==NULL) return(nb_Unknown);
  if((member=member->next)==NULL) return(nb_Unknown);
  if((object2=member->object)==NULL) return(nb_Unknown);
  string=(NB_String *)object1->value;
  real=(NB_Real *)object2->value;
  if(string->object.type!=strType || real->object.type!=realType) return(nb_Unknown);
  if(real->value<0 || real->value>=INT_MAX) return(nb_Unknown);
  i=rint(real->value);
  value=(*function)(string->value,i);
  return((NB_Object *)useString(value));
  }


static NB_Object *evalCall(struct CALL *call){
  NB_CellFunction *cellFunction=call->function;
  NB_CallBinding *callBinding=cellFunction->binding;
  return((*(callBinding->binder))(cellFunction->function,call->list));
  }

static NB_Object *evalCallMod(struct CALL *call){
  NB_List *list=call->list;

  return(bindCallD_DD(fmod,list));
  }

static NB_Object *evalCallTrace(struct CALL *call){
  NB_List *list=call->list;
  NB_Link *member;

  if(list==NULL) return(nb_Unknown);  // 2013-01-13 eat - rosecheckers
  if((member=list->link)==NULL) return(nb_Unknown);
  outPut("TRACE: ");
  printObject(member->object->value);
  outPut(" == ");
  printObject(member->object);
  outPut("\n");
  return(member->object->value);
  }

static void solveCall(struct CALL *call){
  nbCellSolve_((NB_Cell *)call->list);
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
static void enableCall(struct CALL *call){
  nbAxonEnable((NB_Cell *)call->list,(NB_Cell *)call);
  }

static void disableCall(struct CALL *call){
  nbAxonDisable((NB_Cell *)call->list,(NB_Cell *)call);
  }

void destroyCall(NB_Call *call){
  //dropObject(call->function);
  nbAxonDisable((NB_Cell *)call->list,(NB_Cell *)call);
  dropObject(call->list);
  freeCondition((NB_Cond *)call);
  }

/**********************************************************************
* Public Methods
**********************************************************************/
/*
*  Declare a call binding, replacing if name exists
*
*  Returns:  0 - created new, 1 - updated existing
*/
int nbDeclareCallBinding(nbCELL context,char *name,void *binder,int argc){
  NB_CallBinding *callBinding;
  int rc=1;  // assume it exists - probably not

  for(callBinding=callBindings;callBinding!=NULL &&
    strcmp(name,((NB_String *)callBinding->name)->value)!=0;
    callBinding=(NB_CallBinding *)callBinding->object.next);
  if(!callBinding){
    callBinding=nbAlloc(sizeof(NB_CallBinding));
    memset(callBinding,0,sizeof(NB_CallBinding));
    callBinding->name=grabObject(useString(name));
    callBinding->argc=argc;
    callBinding->object.type=nb_DisabledType;  // for now
    callBinding->object.next=(NB_Object *)callBindings;
    callBinding->object.value=(NB_Object *)callBinding;
    callBindings=callBinding;
    rc=0;   // normal to create new
    }
  callBinding->binder=binder;
  return(rc); 
  }

/*
*  Bind a cell function, replacing if name exists
*
*  Returns:  0 - created new, 1 - updated existing
*/
int nbBindCellFunction(nbCELL context,char *name,void *function,char *bindingName){
  NB_CallBinding *callBinding;
  NB_CellFunction *cellFunction;
  int rc=1;  // assume it exists - probably not

  for(callBinding=callBindings;callBinding!=NULL &&
    strcmp(bindingName,((NB_String *)callBinding->name)->value)!=0;
    callBinding=(NB_CallBinding *)callBinding->object.next);
  if(!callBinding){
    outMsg(0,'L',"nbBindCellFunction: called with unregistered binding \"%s\"",bindingName);
    return(-1);
    }
  for(cellFunction=cellFunctions;cellFunction!=NULL &&
    strcmp(name,((NB_String *)cellFunction->name)->value)!=0;
    cellFunction=(NB_CellFunction *)cellFunction->object.next);
  if(!cellFunction){
    cellFunction=nbAlloc(sizeof(NB_CellFunction));
    memset(cellFunction,0,sizeof(NB_CellFunction));
    cellFunction->name=grabObject(useString(name));
    cellFunction->object.type=nb_DisabledType;  // for now
    cellFunction->object.next=(NB_Object *)cellFunctions;
    cellFunction->object.value=(NB_Object *)cellFunction;
    cellFunctions=cellFunction;
    rc=0;   // normal to create new
    }
  cellFunction->function=function;
  cellFunction->binding=callBinding;
  return(rc);
  }

void NbCallInit(NB_Stem *stem){
  callType=nbObjectType(stem,"call",0,TYPE_NO_PAREN,printCall2,destroyCall);
  nbCellType(callType,solveCall,evalCall,enableCall,disableCall);
  callTypeMod=nbObjectType(stem,"mod",0,0,printCall,destroyCondition);
  nbCellType(callTypeMod,solveCall,evalCallMod,enableCall,disableCall);
  callTypeTrace=nbObjectType(stem,"trace",0,0,printCall,destroyCondition);
  nbCellType(callTypeTrace,solveCall,evalCallTrace,enableCall,disableCall);

  nbDeclareCallBinding(NULL,"nb.d(d)",bindCallD_D,1);
  nbDeclareCallBinding(NULL,"nb.d(d,d)",bindCallD_DD,2);
  nbDeclareCallBinding(NULL,"nb.s(s,i)",bindCallS_SI,2);

  double (*funcD_D[26])()={ceil,floor,fabs,exp,log,log10,sqrt,rint,round,trunc,sin,cos,tan,
                          ceil,floor,fabs,exp,log,log10,sqrt,rint,round,trunc,sin,cos,tan};
  char *funcNameD_D[26]={"math.ceil","math.floor","math.abs","math.exp","math.log","math.log10","math.sqrt","math.rint","math.round","math.trunc","math.sin","math.cos","math.tan",
                        "ceil","floor","abs","exp","log","log10","sqrt","rint","round","trunc","sin","cos","tan"}; // second set is for backward compatibility with deprecation notice
  int i;
  for(i=0;i<26;i++){
    nbBindCellFunction(NULL,funcNameD_D[i],funcD_D[i],"nb.d(d)");
    }

  double (*funcD_DD[6])()={fmod,pow,hypot,
                          fmod,pow,hypot};
  char *funcNameD_DD[6]={"math.mod","math.pow","math.hypot",
                        "mod","pow","hypot"};                    // second set is for backwork compatibility with deprecation notice
  for(i=0;i<6;i++){
    nbBindCellFunction(NULL,funcNameD_DD[i],funcD_DD[i],"nb.d(d,d)");
    }
  }

/*
*  Call constructor - we are using useCondition()
*/


/*
*  Find or create function call cell
*/
NB_Object *NbCallUse(nbCELL context,char *ident,NB_List *list){
  NB_CellFunction *cellFunction;
  NB_Link *member;

  for(cellFunction=cellFunctions;cellFunction!=NULL &&
    strcmp(ident,((NB_String *)cellFunction->name)->value)!=0;
    cellFunction=(NB_CellFunction *)cellFunction->object.next);
  if(cellFunction){ // use the registered function if found
    int i=0;
    for(member=list->link;member!=NULL;member=member->next,i++);
    if(i!=cellFunction->binding->argc){
      outMsg(0,'T',"%s expects %d arguments, but %d specified",ident,cellFunction->binding->argc,i);
      return(0);
      }
    return((NB_Object *)useCondition(callType,cellFunction,list));
    }
  if(strcmp(ident,"trace")==0){
    return((NB_Object *)useCondition(callTypeTrace,nb_Unknown,list));
    }
  NB_Type *type;
  for(member=regfun;member!=NULL;member=member->next){
    type=((struct TYPE *)member->object);
    if(strcmp(type->name,ident)==0){
      return((NB_Object *)type->construct(type,list->link));
      }
    }
  return(NULL);
  }
