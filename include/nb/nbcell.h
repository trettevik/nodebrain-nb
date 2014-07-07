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
* File:     nbcell.h 
* 
* Title:    Cell Header
*
* Function:
*
*   This header defines routines that manage NB_Cell structures.  An NB_Cell is
*   an extension of NB_Object.  The NB_Cell structure is further extended by
*   by NB_Term and other structures.
*
* See nbcell.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-04 Ed Trettevik (split out in 0.4.1)
* 2002-09-08 eat - version 0.4.1 A2
*             1) The value pointer has been moved from NB_CELL to NB_OBJECT.
* 2003-11-03 eat 0.5.5  Renamed a lot of functions and variables.
* 2005-04-09 eat 0.6.2  API function definitions moved to nbapi.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2014-01-12 eat 0.9.00 Introduced axon enable and disable routines
* 2014-01-19 eat 0.9.00 Added mode to cell and change level from 2 to 1 byte
*=============================================================================
*/
#ifndef _NB_CELL_H_
#define _NB_CELL_H_

#if defined(NB_INTERNAL)

/* condition function values
*/
extern NB_Object *nb_Disabled;
extern NB_Object *nb_True;
extern NB_Object *nb_False;
extern NB_Object *nb_Unknown;
extern NB_Object *NB_OBJECT_FALSE;
extern NB_Object *NB_OBJECT_TRUE;

extern NB_Link *regfun;
extern NB_Link *change;

typedef struct NB_CELL{      // cell object header
  NB_Object object;          // object header
  struct NB_TREE_NODE  *sub; // subscribers to change
  unsigned char mode;        // mode flags - see NB_CELL_MODE_* below
  unsigned char level;       // subscription level
  } NB_Cell;

typedef NB_Cell *nbCELL;

#define NB_CELL_MODE_SCHEDULED  1  // cell is scheduled for evaluation - avoids scheduling multiple times
#define NB_CELL_MODE_AXON_BOOST 2  // use axon accelerator cell for relational operators
#define NB_CELL_MODE_TIMER      4  // cell has a timer set - cancel when appropriate
#define NB_CELL_MODE_TRANSIENT  8  // alert transient term

void nbCellInit(struct NB_STEM *stem);
void nbCellType(
  NB_Type *type,
  void (*solve)(),
  NB_Object * (*eval)(),
  void (*enable)(),
  void (*disable)());
void nbCellTypeSub(
  NB_Type *type,
  int reg,
  NB_Object *(*parse)(),
  NB_Object *(*construct)(),
  double (*evalDouble)(),
  char *(*evalString)());
void *nbCellNew(NB_Type *type,void **pool,int length);
NB_Object *nbCellSolve_(NB_Cell *cell);
NB_Object *nbCellCompute_(NB_Cell *cell);
void nbCellShowSub(NB_Cell *cell);
void nbCellShowImpact(NB_Cell *cell);
void nbCellLevel(NB_Cell *pub);
void nbCellAlert(NB_Cell *cell);
void nbCellReact(void);

#else  // !NB_INTERNAL (exernal interface)

typedef void*  nbCELL;        /* void the API cell pointer */

#endif // !NB_INTERNAL (external interface)

//************************************
// External API

/* We need to replace these with references to NB_TYPE_* values */

#if defined(WIN32)
#if defined(LIBNB_EXPORTS)
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
#endif
extern nbCELL NB_CELL_DISABLED;

#if defined(WIN32)
#if defined(LIBNB_EXPORTS)
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
#endif
extern nbCELL NB_CELL_UNKNOWN;

#if defined(WIN32)
#if defined(LIBNB_EXPORTS)
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
#endif
extern nbCELL NB_CELL_PLACEHOLDER;

#if defined(WIN32)
#if defined(LIBNB_EXPORTS)
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
#endif
extern nbCELL NB_CELL_FALSE;

#if defined(WIN32)
#if defined(LIBNB_EXPORTS)
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
#endif
extern nbCELL NB_CELL_TRUE;

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellObtain(nbCELL context,char *cellExpression);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellCreate(nbCELL context,char *cellExpression);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellParse(nbCELL context,char **cellExpression);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellGetValue(nbCELL context,nbCELL cell);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellEnable(nbCELL pub,nbCELL sub);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellDisable(nbCELL pub,nbCELL sub);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbCellGetType(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellGrab(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellDrop(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellRelease(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellPub(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellFree(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellShow(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellEvaluate(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellCompute(nbCELL context,nbCELL cell);
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellSolve(nbCELL context,nbCELL cell);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellPublish(nbCELL pub);

/* API String Functions */
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellObtainString(nbCELL context,char *string);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellCreateString(nbCELL context,char *string);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern char *nbCellGetString(nbCELL context,nbCELL cell);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern char *nbCellGetText(nbCELL context,nbCELL cell);

/* API Real Functions */
#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellObtainReal(nbCELL context,double real);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbCellCreateReal(nbCELL context,double real);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern double nbCellGetReal(nbCELL context,nbCELL cell);

#endif
