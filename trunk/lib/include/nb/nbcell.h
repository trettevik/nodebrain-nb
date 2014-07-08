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
*=============================================================================
*/
#ifndef _NB_CELL_H_
#define _NB_CELL_H_

#if defined(NB_INTERNAL)

/* condition function values
*/
extern NB_Object *nb_Disabled;
extern NB_Object *nb_Unknown;
extern NB_Object *NB_OBJECT_FALSE;
extern NB_Object *NB_OBJECT_TRUE;

extern NB_Link *regfun;
extern NB_Link *change;

struct NB_CELL{            /* Function object header */
  NB_Object object;        /* object header */
  //struct NB_LINK  *sub;    /* subscribers to change */
  struct NB_TREE_NODE  *sub;    /* subscribers to change */
  unsigned short  level;   /* subscription level */
  };

typedef struct NB_CELL NB_Cell;
typedef NB_Cell *nbCELL;

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
//extern void nbCellEnable(nbCELL pub,NB_Object *sub);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCellDisable(nbCELL pub,nbCELL sub);
//extern void nbCellDisable(nbCELL pub,NB_Object *sub);

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
extern nbCELL nbCellCreateReal(nbCELL context,double real);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern double nbCellGetReal(nbCELL context,nbCELL cell);

#endif
