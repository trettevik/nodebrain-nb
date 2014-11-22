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
* File:     nbmath.c 
*
* Title:    Real Number Math Methods
*
* Function:
*
*   This file provides methods for nodebrain REAL objects, implemented as
*   DOUBLE in C.  The REAL type extends the OBJECT type defined in nbobject.h.
*   The MATH object extends NB_Cell as defined in nbcell.h.
*
* Synopsis:
*
*   #include "nbmath.h"
*
*   void initMath();
*   struct MATH *useMath(NB_Type *type,NB_Object *left,NB_Object *right);
*   void printMath();
*   void printMathAll();
* 
* Description
*
*   Real objects may be constants or variables.  Constants may be shared
*   by many objects in nodebrain memory.  Obtain a pointer to a constant
*   real object by calling useReal().  A new object is created if necessary.
*
*     struct REAL *myreal=useReal(double);
*
*   When assigning a constant real object pointer to another data structure,
*   reserve the real object by calling grabObject().  This increments a reference
*   count in the real object.
*
*     mystruct.realPtr=grabObject(useReal(10.987));
*
*   When a reference to a constant real object is no longer needed, release
*   the reservation by calling dropObject().
*
*     dropObject(struct REAL *myreal);
*
*   When all references to a constant real are dropped, the memory assigned to
*   the object may be reused.
*
*=============================================================================
* Enhancements:
*
*   Include data conversion methods for each object type.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 Ed Trettevik (original prototype version introduced in 0.4.1)
* 2004-01-08 eat 0.6.0  Removed imbedded REAL from MATH structure.
*            This change makes math a bit less efficient because computed
*            values must be looked up by useReal() instead of simply being
*            stuffed in the imbedded structure.  But it simplifies the
*            management of "grabs" on non-imbedded reals and the recognition
*            of value changes via address changes.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-12-27 eat 0.8.13 Checker updates
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
* 2014-05-04 eat 0.9.02 Replaced references to realType to NB_OBJECT_KIND_REAL
* 2014-11-20 eat 0.9.03 Included nbFunctionDD and nbFunctionDDD
*            These API functions enable a module to register additional
*            math functions.
*=============================================================================
*/
#include <nb/nbi.h>

struct MATH *mathFree=NULL;

struct TYPE *mathTypeInv;
struct TYPE *mathTypeAdd;
struct TYPE *mathTypeSub;
struct TYPE *mathTypeMul;
struct TYPE *mathTypeDiv;

/**********************************************************************
* Private Object Methods
**********************************************************************/
static void printMath(struct MATH *math){
  if(math==NULL){
    outPut("(?)");
    return;
    }
  outPut("(");
  if(math->left!=NULL){
    if(math->left==nb_False){
      outPut("(");
      printObject(math->left);
      outPut(")");
      }
    else printObject(math->left);
    }
  outPut("%s",math->cell.object.type->name);
  if(math->right==nb_False || math->right==nb_True){
    outPut("(");
    printObject(math->right);
    outPut(")");
    }
  else printObject(math->right);
  outPut(")");
  }

static void printMathX(struct MATH *math){
  outPut("`%s",math->cell.object.type->name);
  outPut("(");
  printObject(math->right);
  outPut(")");
  }

static void printMathXY(struct MATH *math){
  outPut("`%s",math->cell.object.type->name);
  outPut("(");
  printObject(math->left);
  outPut(",");
  printObject(math->right);
  outPut(")");
  }

/*
*  Math destructor
*/
static void destroyMath(NB_Math *math){
  struct MATH *lmath,**mathP;
  NB_Hash *hash=math->cell.object.type->hash;
  uint32_t hashcode;

  if(trace) outMsg(0,'T',"destroyMath() called");
  //hashcode=hashCond(math->cell.object.type,math->left,math->right);
  hashcode=math->cell.object.hashcode;
  mathP=(struct MATH **)&(hash->vect[hashcode&hash->mask]);
  if(*mathP==math) *mathP=(struct MATH *)math->cell.object.next;
  else{
    for(lmath=*mathP;lmath!=NULL && lmath!=math;lmath=*mathP)
      mathP=(struct MATH **)&lmath->cell.object.next;
    if(lmath==math) *mathP=(struct MATH *)math->cell.object.next;
    }
  nbAxonDisable((NB_Cell *)math->left,(NB_Cell *)math);
  dropObject(math->left);
  nbAxonDisable((NB_Cell *)math->right,(NB_Cell *)math);
  dropObject(math->right);
  math->cell.object.next=(NB_Object *)mathFree;
  mathFree=math;
  hash->objects--;
  if(trace) outMsg(0,'T',"destroyMath() returning");
  }

/**********************************************************************
* Private Function Calculation Methods
**********************************************************************/
/*
*  These functions should eventually use common routines to convert
*  the operands to real from 
*
*       real, math, condition, integer, string, term, ...
*
*    For example, evalMathAdd() would look like this:
*
*       if((realP=likeReal(math->left))==NULL) return(Unknown);
*       value=(*realP)->value;  # save value
*       if((realP=likeReal(math->left))==NULL) return(Unknown);
*       math->value.value=value+(*realP)->value;      
*       return(&math->value.object);
*
*    We could also have a common function that handles the conversions
*    and returnes a structure with a pair of real values.
*
*    Preferably we would make it the responsibility of each object type
*    to provide a method for returning a value in any of the common
*    data types: real, integer, string.  These functions for a term
*    simply forward the request to the assigned data type.
*
*    As it is, we have silly duplicated code, but it was quick.
*
*  NOTE: Our change reaction engine (nbEval) responds changes in a
*  function's value pointer.  Because math functions point to themselves
*  and the pointer is unchanging, they place a pointer to Unknown
*  in their function value pointer, when they want the engine to
*  recognize a change.
*       
*/
static NB_Object *evalMathX(struct MATH *math){
  struct MATH *rmath=(struct MATH *)math->right;
  struct REAL *rreal;
  
  rreal=(struct REAL *)rmath->cell.object.value;
  if(rreal->object.type->kind&=NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal((math->cell.object.type->evalDouble)(rreal->value)));
  return(nb_Unknown);
  }

static NB_Object *evalMathXY(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->object.type->kind&NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal((math->cell.object.type->evalDouble)(lreal->value,rreal->value)));
  return(nb_Unknown);
  }

static NB_Object *evalMathInv(struct MATH *math){
  struct MATH *rmath=(struct MATH *)math->right;
  struct REAL *rreal;

  rreal=(struct REAL *)rmath->cell.object.value;
  if(rreal->object.type->kind&=NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal(-rreal->value));
  else return(nb_Unknown);
  }

static NB_Object *evalMathAdd(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathAdd() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->object.type->kind&NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal(lreal->value+rreal->value));
  return(nb_Unknown);
  }

static NB_Object *evalMathSub(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathSub() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->object.type->kind&NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal(lreal->value-rreal->value));
  return(nb_Unknown);
  }

static NB_Object *evalMathMul(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathMul() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->object.type->kind&NB_OBJECT_KIND_REAL)
    return((NB_Object *)useReal(lreal->value*rreal->value));
  return(nb_Unknown);
  }

static NB_Object *evalMathDiv(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathDiv() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->object.type->kind&NB_OBJECT_KIND_REAL && rreal->value!=0)
    return((NB_Object *)useReal(lreal->value/rreal->value));
  return(nb_Unknown);
  }

/*
*   Note: solveMath() is identical to solveInfix2() for conditions
*         so we could use solveInfix2() instead
*/
static void solveMath(struct MATH *math){
  if(((NB_Object *)math->left)->value==nb_Unknown){
    if(nbCellSolve_((NB_Cell *)math->left)==nb_Unknown) return;
    } 
  if(((NB_Object *)math->right)->value==nb_Unknown) nbCellSolve_((NB_Cell *)math->right);   
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
static void enableMath(struct MATH *math){
  nbAxonEnable((NB_Cell *)math->left,(NB_Cell *)math);
  nbAxonEnable((NB_Cell *)math->right,(NB_Cell *)math);
  }

static void disableMath(struct MATH *math){
  nbAxonDisable((NB_Cell *)math->left,(NB_Cell *)math);
  nbAxonDisable((NB_Cell *)math->right,(NB_Cell *)math);
  }

/*
*  Sub Function Constructors
*/
static NB_Object *mathConX(struct TYPE *type,NB_Link *member){
  NB_Object *right;
  if(member==NULL) return(NULL);
  right=member->object;
  return((NB_Object *)useMath(type,nb_Unknown,right));
  }

static NB_Object *mathConXY(struct TYPE *type,NB_Link *member){
  NB_Object *left,*right;
  if(member==NULL) return(NULL);
  left=member->object;
  if((member=member->next)==NULL) return(NULL);
  right=member->object;
  return((NB_Object *)useMath(type,left,right));
  }

/**********************************************************************
* Public Methods
**********************************************************************/
int nbFunctionD_D(nbCELL context,char *name,double (*function)(double x)){
  NB_Stem *stem=context->object.type->stem;
  NB_Type *type;
  type=nbObjectType(stem,name,0,TYPE_IS_MATH,printMathX,destroyMath);
  nbCellType(type,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(type,1,NULL,mathConX,function,NULL);
  return(0);
  }

int nbFunctionD_DD(nbCELL context,char *name,double (*function)(double x,double y)){
  NB_Stem *stem=context->object.type->stem;
  NB_Type *type;
  type=nbObjectType(stem,name,0,TYPE_IS_MATH,printMathXY,destroyMath);
  nbCellType(type,solveMath,evalMathXY,enableMath,disableMath);
  nbCellTypeSub(type,1,NULL,mathConXY,function,NULL);
  return(0);
  }

void initMath(NB_Stem *stem){
  mathTypeInv=nbObjectType(stem,"-",0,TYPE_IS_MATH,printMath,destroyMath);
  nbCellType(mathTypeInv,solveMath,evalMathInv,enableMath,disableMath);
  mathTypeAdd=nbObjectType(stem,"+",0,TYPE_IS_MATH,printMath,destroyMath);
  nbCellType(mathTypeAdd,solveMath,evalMathAdd,enableMath,disableMath);
  mathTypeSub=nbObjectType(stem,"-",0,TYPE_IS_MATH,printMath,destroyMath);
  nbCellType(mathTypeSub,solveMath,evalMathSub,enableMath,disableMath);
  mathTypeMul=nbObjectType(stem,"*",0,TYPE_IS_MATH,printMath,destroyMath);
  nbCellType(mathTypeMul,solveMath,evalMathMul,enableMath,disableMath);
  mathTypeDiv=nbObjectType(stem,"/",0,TYPE_IS_MATH,printMath,destroyMath);
  nbCellType(mathTypeDiv,solveMath,evalMathDiv,enableMath,disableMath);

  NB_Type *type;
  // arrary of function defined as:  double f(double x)
  double (*funcDD[13])()={ceil,floor,fabs,exp,log,log10,sqrt,rint,round,trunc,sin,cos,tan};
  char *funcNameDD[13]={"math.ceil","math.floor","math.abs","math.exp","math.log","math.log10","math.sqrt","math.rint","math.round","math.trunc","math.sin","math.cos","math.tan"};
  int i;
  for(i=0;i<13;i++){
    type=nbObjectType(stem,funcNameDD[i],0,TYPE_IS_MATH,printMathX,destroyMath);
    nbCellType(type,solveMath,evalMathX,enableMath,disableMath);
    nbCellTypeSub(type,1,NULL,mathConX,funcDD[i],NULL);
    }
  double (*funcDDD[3])()={fmod,pow,hypot};
  char *funcNameDDD[3]={"math.mod","math.pow","math.hypot"};
  for(i=0;i<3;i++){
    type=nbObjectType(stem,funcNameDDD[i],0,TYPE_IS_MATH,printMathXY,destroyMath);
    nbCellType(type,solveMath,evalMathXY,enableMath,disableMath);
    nbCellTypeSub(type,1,NULL,mathConXY,funcDDD[i],NULL);
    }
  }

/*
*  Math constructor
*/
NB_Math *useMath(NB_Type *type,NB_Object *left,NB_Object *right){
  /*
  *  This routine returns a pointer to a math object.  If the
  *  expression is already defined, the existing structure is returned.
  *  Otherwise a new math object is constructed.
  *
  *  Expressions are "disabled" until they are referenced by a rule.  Such
  *  a reference is reported by nbAxonEnable which builds the back link list
  *  to dependent expressions referenced by a rule.  This prevents the
  *  eval routine from evaluating conditions not referenced by any
  *  rule.
  *
  */
  struct MATH *math,*lmath,**mathP;
  NB_Hash *hash=type->hash;
  uint32_t hashcode;

  if(trace) outMsg(0,'T',"useMath: called");

  hashcode=hashCond(type,left,right);
  mathP=(struct MATH **)&(hash->vect[hashcode&hash->mask]);
  for(math=*mathP;math!=NULL;math=*mathP){
    if(math->left==left && math->right==right && math->cell.object.type==type) return(math);
    mathP=(struct MATH **)&math->cell.object.next;
    }
  math=nbCellNew(type,(void **)&mathFree,sizeof(struct MATH));
  math->cell.object.hashcode=hashcode;
  math->cell.object.next=(NB_Object *)*mathP;
  *mathP=math;
  hash->objects++;
  if(hash->objects>=hash->limit) nbHashGrow(&type->hash);
  math->left=grabObject(left);
  math->right=grabObject(right);
  if((lmath=(struct MATH *)left)!=(struct MATH *)nb_Unknown && lmath->cell.object.value!=(NB_Object *)lmath)
    math->cell.level=lmath->cell.level+1;
  if((lmath=(struct MATH *)right)!=(struct MATH *)nb_Unknown && lmath->cell.object.value!=(NB_Object *)lmath &&
    math->cell.level<=lmath->cell.level)
    math->cell.level=lmath->cell.level+1;
  math->cell.object.value=nb_Disabled;
  return(math);
  }

void printMathAll(void){
  NB_Type *type;

  for(type=nb_TypeList;type!=NULL;type=(NB_Type *)type->object.next){
    if(type->attributes&TYPE_IS_MATH) nbHashShow(type->hash,type->name,type); 
    }
  }
