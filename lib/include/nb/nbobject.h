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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbobject.h 
* 
* Title:    Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain objects.
*
* Note:
*
*   Although a cell is an extension of an object, we have come to view "object"
*   as a special case of "cell".  Functions that operate on cells can recognize
*   a "simple" object as a special case where the value points to the object
*   itself.  Because of this awareness, most functions will accept any object
*   as a cell.  If we make a biological comparison, what we call a cell is a
*   special kind of cell called a "neuron".  Each of the these cells has "dendrite"
*   for bringing information in (methods provided by the cell type) and "axon"
*   for taking information out (subscription list).  What we call an object
*   here is a neuron that lacks our standard "axon".  It can take information in from
*   other cells by its methods, but has no "axon hillock" (subscription list)
*   for sending information out.  However, there is nothing preventing these
*   limited cells from using "proprietary" methods of reacting to the information
*   received by the dendrite.  Furthermore, these limited cells may in fact
*   respond using their own construction of axon that provides a path to the
*   dendrite of other cells. 
*
* See nbobject.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-31 Ed Trettevik (split out in version 0.4.1)
* 2002-09-08 eat - version 0.4.1 A2
*             1) Included value pointer in object structure so it is common
*                to all objects.  Object types that don't compute values
*                can just point to themselves.  This makes it simple for 
*                methods to grab a value pointer from an object, at the
*                expense of a little memory.
* 2005-04-08 eat 0.6.2  API functions definitions move to nbapi.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2014-01-12 eat 0.9.00 Included more type flags
*=============================================================================
*/
#ifndef _NB_OBJECT_H_
#define _NB_OBJECT_H_

//#include <nb/nbstem.h>

#if defined(NB_INTERNAL)

#define NB_OBJECT_MANAGED_SIZE 4096  // malloc is used for objects larger than this

extern int showvalue;
extern int showstate;
extern int showlevel;
extern int showcount;

struct NB_OBJECT{             /* object header */
  struct   NB_OBJECT *next;   /* link to next object in hash list */
  struct   TYPE      *type;   /* object type */
  struct   NB_OBJECT *value;  /* value pointer - may point to self */
  int32_t  refcnt;            /* number of references */
  uint32_t key;               /* hashing key */
  };

typedef struct NB_OBJECT NB_Object;

struct NB_LINK{                 /* object list link */
  struct NB_LINK *next;         /* next link */
  struct NB_OBJECT *object;     /* member object */  
  };

typedef struct NB_LINK NB_Link;

//extern NB_Object *nb_Unknown;
extern NB_Object *nb_Undefined;
extern NB_Object *nb_Placeholder;

                           /* object type attributes - bit mask */
#define TYPE_ENABLES    2  /* Qualifies for enable/disable command */
#define TYPE_RULE       6  /* rule: 4+2 - subscriber */
#define TYPE_IS_RULE    4
#define TYPE_REL        8  /* relational: 8+3 - publish and subscribe */
#define TYPE_IS_REL     8
#define TYPE_COMP      16  /* compare:(Test and Cmp) 16+3 */
#define TYPE_IS_COMP   16 
#define TYPE_BOOL      32  /* boolean:(not|and|or) 32+3 */
#define TYPE_IS_BOOL   32
#define TYPE_TIME      64
#define TYPE_IS_TIME   64
#define TYPE_DELAY    128
#define TYPE_IS_DELAY 128 
#define TYPE_SPECIAL  256  /* special values besides NULL */
#define TYPE_REGEXP   512  /* regular expression */
#define TYPE_WELDED  1024  /* terms should not release these objects easily */
                           /* - an explicit undefine is required */
#define TYPE_IS_FACT   0x0800   /* facts */
#define TYPE_NOT_TRUE  0x1000   // Objects like zero, unknown, and disabled that are not true
#define TYPE_IS_ASSERT 0x2000   /* assertions */
#define TYPE_IS_MATH   0x4000   // Math functions

typedef struct TYPE{
  struct NB_OBJECT object;     /* object header */
  struct NB_STEM   *stem;      /* brain stem cell */ 
  char *name;                  /* symbolic name */
  struct HASH *hash;           /* hashing table for object lookup */
  int  attributes;             /* see object type attributes above */
  int  apicelltype;            /* cell type code for API */
  void (*showExpr)();          /* show as expression */
  void (*showItem)();          /* show as list item */
  void (*showReport)();        /* show as report - may include \n */
  void (*destroy)();           /* destructor method */
  void (*alert)();             /* alert handler - defaults to nbCellAlert() for cells */
  void (*alarm)();             /* clock alarm handler - defaults to nbCellAlert() for cells */
  struct NB_OBJECT *(*parse)();   /* parsing routine - will enable modular parsing */
  struct NB_OBJECT *(*construct)();/* Constructor subroutine */
  /* cell methods */
  void (*solve)();             /* solve for unknown values - find out what you don't know */
  struct NB_OBJECT *(*compute)(); /* compute a value - use what you have indirectly available */
  struct NB_OBJECT *(*eval)();    /* reactive evaluation method - use what is provided by alerts */
  //struct NB_OBJECT *(*eval_)();   /* Used by nbCellEvalTrace() to forward request */
  //clock_t eval_ticks;          // clock ticks in eval routine (if shimmed)
  double (*evalDouble)();      /* Number evaluation subroutine - eval can handle function family */
  char *(*evalString)();       /* String evaluation subroutine */
  void (*enable)();            /* resubscribe method */
  void (*disable)();           /* subscription cancellation method */
  struct NB_TYPE_SHIM *shim;   // trace shim
  } NB_Type;
 
struct NB_TYPE_SHIM{           // type method trace shim
  void (*showExpr)();          /* show as expression */
  void (*showItem)();          /* show as list item */
  void (*showReport)();        /* show as report - may include \n */
  void (*destroy)();           /* destructor method */
  void (*alert)();             /* alert handler - defaults to nbCellAlert() for cells */
  int     alertFlags;          // option flags for alert shim
  clock_t alertTicks;          // clock ticks in alert routine
  void (*alarm)();             /* clock alarm handler - defaults to nbCellAlert() for cells */
  int     alarmFlags;          // option flags for alarm shim
  clock_t alarmTicks;          // clock ticks in alarm routine
  struct NB_OBJECT *(*parse)();   /* parsing routine - will enable modular parsing */
  struct NB_OBJECT *(*construct)();/* Constructor subroutine */
  /* cell methods */
  void (*solve)();             /* solve for unknown values - find out what you don't know */
  struct NB_OBJECT *(*compute)(); /* compute a value - use what you have indirectly available */
  struct NB_OBJECT *(*eval)();    /* reactive evaluation method - use what is provided by alerts */
  int     evalFlags;           // option flags for eval shim
  clock_t evalTicks;           // clock ticks in eval routine
  double (*evalDouble)();      /* Number evaluation subroutine - eval can handle function family */
  char *(*evalString)();       /* String evaluation subroutine */
  void (*enable)();            /* resubscribe method */
  void (*disable)();           /* subscription cancellation method */
  };

NB_Type *nb_TypeList;

extern NB_Type *nb_DisabledType; /* Special object type */
extern NB_Type *nb_FalseType;    /* Special object type */
extern NB_Type *nb_UnknownType;  /* Special object type */
extern NB_Type *nb_DefinedType;  /* Special object type */
extern NB_Type *nb_TypeType;     /* Type object type */

struct TYPE *newType(struct NB_STEM *stem,char *name,struct HASH *hash,int  attributes,void (*showExpr)(),void (*destroy)());
void enableBug(NB_Object *object);
void disableBug(NB_Object *object);


void *newObject(struct TYPE *type,void **pool,int size);
void *grabObject(void *object);
void *grabObjectNull(void *object);
void *dropObject(void *object);
void *dropObjectNull(void *object);
void printObject(NB_Object *object);
void printObjectItem(NB_Object *object);
void printObjectReport(NB_Object *object);

//void nbObjectPrintSpecial();
void nbObjectInit(struct NB_STEM *stem);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern
void *nbAlloc(int size);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbFree(void *object,int size);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbObjectShowTypes(void);

#endif // NB_INTERNAL

//*********************************
// External API

#define NB_TYPE_UNDEFINED    0     /* API Cell Types */
#define NB_TYPE_DISABLED     1
#define NB_TYPE_UNKNOWN      2
#define NB_TYPE_PLACEHOLDER  3
#define NB_TYPE_STRING       4
#define NB_TYPE_REAL         5
#define NB_TYPE_LIST         6
#define NB_TYPE_TERM         7
#define NB_TYPE_NODE         8
#define NB_TYPE_VERB         9
#define NB_TYPE_TEXT        10
#define NB_TYPE_FALSE       11    // 2013-12-05 eat - no longer zero

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbHeap();

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void *nbAlloc(int size);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbFree(void *object,int size);

#endif
