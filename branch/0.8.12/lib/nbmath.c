/*
* Copyright (C) 1998-2010 The Boeing Company
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
*   struct STRING *useMath(char *string);
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
*=============================================================================
*/
#include <nb/nbi.h>

void *hashCond();

struct HASH *mathH;
struct MATH *mathFree=NULL;

struct TYPE *mathTypeInv;
struct TYPE *mathTypeAdd;
struct TYPE *mathTypeSub;
struct TYPE *mathTypeMul;
struct TYPE *mathTypeDiv;

struct TYPE *mathTypeCeil;
struct TYPE *mathTypeFloor;
struct TYPE *mathTypeAbs;
/*
struct TYPE *mathTypeRint;
*/
struct TYPE *mathTypeExp;
struct TYPE *mathTypeLog;
struct TYPE *mathTypeLog10;
struct TYPE *mathTypeSqrt;
struct TYPE *mathTypeHypot;
struct TYPE *mathTypeMod;
struct TYPE *mathTypePow;


/**********************************************************************
* Private Object Methods
**********************************************************************/
void printMath(math) struct MATH *math;{
  if(math==NULL) outPut("(?)");
/*
  else if(*math->cell.object.type->name>='a' && *math->cell.object.type->name<='z'){
    outPut("%s",math->cell.object.type->name);
    outPut("(");
    if(math->left!=NULL){
      printObject(math->left);
      outPut(",");
      }
    printObject(math->right);
    outPut(")");
    }
  else{
*/
    outPut("(");
    if(math->left!=NULL) printObject(math->left);
    outPut("%s",math->cell.object.type->name);
    printObject(math->right);
    outPut(")");
//    }
  }

void printMathX(struct MATH *math){
  outPut("%s",math->cell.object.type->name);
  outPut("(");
  printObject(math->right);
  outPut(")");
  }

void printMathXY(struct MATH *math){
  outPut("%s",math->cell.object.type->name);
  outPut("(");
  printObject(math->left);
  outPut(",");
  printObject(math->right);
  outPut(")");
  }

/*
*  Math destructor
*/
void destroyMath(struct MATH *math){
  struct MATH *lmath,**mathP;

  if(trace) outMsg(0,'T',"destroyMath() called");
  mathP=hashCond(mathH,math->cell.object.type,math->left,math->right);
  if(*mathP==math) *mathP=(struct MATH *)math->cell.object.next;
  else{
    for(lmath=*mathP;lmath!=NULL && lmath!=math;lmath=*mathP)
      mathP=(struct MATH **)&lmath->cell.object.next;
    if(lmath==math) *mathP=(struct MATH *)math->cell.object.next;
    }
  nbCellDisable((NB_Cell *)math->left,(NB_Cell *)math);
  dropObject(math->left);
  nbCellDisable((NB_Cell *)math->right,(NB_Cell *)math);
  dropObject(math->right);
  math->cell.object.next=(NB_Object *)mathFree;
  mathFree=math;
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
NB_Object *evalMathX(struct MATH *math){
  struct MATH *rmath=(struct MATH *)math->right;
  struct REAL *rreal;
  
  rreal=(struct REAL *)rmath->cell.object.value;
  if(rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal((math->cell.object.type->evalDouble)(rreal->value)));
  }

NB_Object *evalMathXY(struct MATH *math){
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal((math->cell.object.type->evalDouble)(lreal->value,rreal->value)));
  }

NB_Object *evalMathInv(math) struct MATH *math; {
  struct MATH *rmath=(struct MATH *)math->right;
  struct REAL *rreal;

  rreal=(struct REAL *)rmath->cell.object.value;
  if(rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal(-rreal->value));
  }

NB_Object *evalMathAdd(math) struct MATH *math; {
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathAdd() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal(lreal->value+rreal->value));
  }

NB_Object *evalMathSub(math) struct MATH *math; {
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathSub() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal(lreal->value-rreal->value));
  }

NB_Object *evalMathMul(math) struct MATH *math; {
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathMul() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  return((NB_Object *)useReal(lreal->value*rreal->value));
  }

NB_Object *evalMathDiv(math) struct MATH *math; {
  struct MATH *lmath=(struct MATH *)math->left,*rmath=(struct MATH *)math->right;
  struct REAL *lreal,*rreal;

  if(trace) outMsg(0,'T',"evalMathDiv() called");
  lreal=(struct REAL *)lmath->cell.object.value;
  rreal=(struct REAL *)rmath->cell.object.value;
  if(lreal->object.type!=realType || rreal->object.type!=realType) return(nb_Unknown);
  if(rreal->value==0) return(nb_Unknown);
  return((NB_Object *)useReal(lreal->value/rreal->value));
  }

/*
*   Note: solveMath() is identical to solveInfix2() for conditions
*         so we could use solveInfix2() instead
*/
void solveMath(struct MATH *math){
  if(((NB_Object *)math->left)->value==nb_Unknown){
    if(nbCellSolve_((NB_Cell *)math->left)==nb_Unknown) return;
    } 
  if(((NB_Object *)math->right)->value==nb_Unknown) nbCellSolve_((NB_Cell *)math->right);   
  return;
  }

/**********************************************************************
* Private Function Management Methods
**********************************************************************/
void enableMath(struct MATH *math){
  nbCellEnable((NB_Cell *)math->left,(NB_Cell *)math);
  nbCellEnable((NB_Cell *)math->right,(NB_Cell *)math);
  }

void disableMath(struct MATH *math){
  nbCellDisable((NB_Cell *)math->left,(NB_Cell *)math);
  nbCellDisable((NB_Cell *)math->right,(NB_Cell *)math);
  }

/*
*  Sub Function Constructors
*/
NB_Object *mathConX(struct TYPE *type,NB_Link *member){
  NB_Object *right;
  if(member==NULL) return(NULL);
  right=member->object;
  return((NB_Object *)useMath(0,type,nb_Unknown,right));
  }

NB_Object *mathConXY(struct TYPE *type,NB_Link *member){
  NB_Object *left,*right;
  if(member==NULL) return(NULL);
  left=member->object;
  if((member=member->next)==NULL) return(NULL);
  right=member->object;
  return((NB_Object *)useMath(0,type,left,right));
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void initMath(NB_Stem *stem){
  mathH=newHash(2031);  /* initialize math hash */
  mathTypeInv=newType(stem,"-",mathH,0,printMath,destroyMath);
  nbCellType(mathTypeInv,solveMath,evalMathInv,enableMath,disableMath);
  mathTypeAdd=newType(stem,"+",mathH,0,printMath,destroyMath);
  nbCellType(mathTypeAdd,solveMath,evalMathAdd,enableMath,disableMath);
  mathTypeSub=newType(stem,"-",mathH,0,printMath,destroyMath);
  nbCellType(mathTypeSub,solveMath,evalMathSub,enableMath,disableMath);
  mathTypeMul=newType(stem,"*",mathH,0,printMath,destroyMath);
  nbCellType(mathTypeMul,solveMath,evalMathMul,enableMath,disableMath);
  mathTypeDiv=newType(stem,"/",mathH,0,printMath,destroyMath);
  nbCellType(mathTypeDiv,solveMath,evalMathDiv,enableMath,disableMath);

  mathTypeCeil=newType(stem,"ceil",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeCeil,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeCeil,1,NULL,mathConX,ceil,NULL);
  mathTypeFloor=newType(stem,"floor",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeFloor,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeFloor,1,NULL,mathConX,floor,NULL);
  mathTypeAbs=newType(stem,"abs",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeAbs,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeAbs,1,NULL,mathConX,fabs,NULL);
/*
  mathTypeRint=newType(stem,"rint",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeRint,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeRint,1,NULL,mathConX,rint,NULL);
*/
  mathTypeExp=newType(stem,"exp",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeExp,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeExp,1,NULL,mathConX,exp,NULL);
  mathTypeLog=newType(stem,"log",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeLog,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeLog,1,NULL,mathConX,log,NULL);
  mathTypeLog10=newType(stem,"log10",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeLog10,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeLog10,1,NULL,mathConX,log10,NULL);
  mathTypeSqrt=newType(stem,"sqrt",mathH,0,printMathX,destroyMath);
  nbCellType(mathTypeSqrt,solveMath,evalMathX,enableMath,disableMath);
  nbCellTypeSub(mathTypeSqrt,1,NULL,mathConX,sqrt,NULL);

#if !defined mpe
  mathTypeHypot=newType(stem,"hypot",mathH,0,printMathXY,destroyMath);
  nbCellType(mathTypeHypot,solveMath,evalMathXY,enableMath,disableMath);
  nbCellTypeSub(mathTypeHypot,1,NULL,mathConXY,hypot,NULL);
#endif
  mathTypeMod=newType(stem,"mod",mathH,0,printMathXY,destroyMath);
  nbCellType(mathTypeMod,solveMath,evalMathXY,enableMath,disableMath);
  nbCellTypeSub(mathTypeMod,1,NULL,mathConXY,fmod,NULL);
  mathTypePow=newType(stem,"pow",mathH,0,printMathXY,destroyMath);
  nbCellType(mathTypePow,solveMath,evalMathXY,enableMath,disableMath);
  nbCellTypeSub(mathTypePow,1,NULL,mathConXY,pow,NULL);
  }

/*
*  Math constructor
*/
struct MATH * useMath(int inverse,struct TYPE *type,NB_Object *left,NB_Object *right){
  /*
  *  This routine returns a pointer to a math object.  If the
  *  expression is already defined, the existing structure is returned.
  *  Otherwise a new math object is constructed.
  *
  *  Expressions are "disabled" until they are referenced by a rule.  Such
  *  a reference is reported by nbCellEnable which builds the back link list
  *  to dependent expressions referenced by a rule.  This prevents the
  *  eval routine from evaluating conditions not referenced by any
  *  rule.
  *
  */
  struct MATH *math,*lmath,**mathP;
  if(trace) outMsg(0,'T',"useMath called");
  if(inverse){
    lmath=useMath(0,type,left,right);
    return(useMath(0,mathTypeInv,(NB_Object *)lmath,NULL));
    }
  mathP=hashCond(mathH,type,left,right);
  for(math=*mathP;math!=NULL;math=*mathP){
    if(math->left==left && math->right==right && math->cell.object.type==type) return(math);
    mathP=(struct MATH **)&math->cell.object.next;
    }
  math=nbCellNew(type,(void **)&mathFree,sizeof(struct MATH));
  math->cell.object.next=(NB_Object *)*mathP;
  *mathP=math;
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
  printHash(mathH,"Math Table",NULL);
  }
