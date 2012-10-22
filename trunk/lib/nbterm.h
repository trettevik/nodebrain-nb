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
* File:     nbterm.h 
* 
* Title:    Term Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage NodeBrain term cells.
*
* See nbterm.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/08/31 Ed Trettevik (original split out from other components)
* 2005/04/08 eat 0.6.2  API function definitions moved to nbapi.h
* 2008/02/08 eat 0.6.9  Glossary of terms changed to a binary tree
* 2010/02/28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_TERM_H_
#define _NB_TERM_H_

#include <nbcell.h>
#include <nbstem.h>

#if defined(NB_INTERNAL)

/* For type identification, class should be phased out.  The term.def->type
*  provides the type of object associated with a term.
*
*  For inheritance, class has been replaced by NB_NODE.REF
*/
typedef struct NB_TERM{
  struct NB_CELL   cell;    /* cell object header */
  // the next four entries are a tree node
  // if this experiment works, we need to replace with
  // struct NB_TreeNode   and reference word ast treeNode.key
  void *left;
  void *right;
  signed int balance;
  struct STRING    *word;   /* word defined within the context */
  struct NB_TERM   *context;/* parent context term */
  struct NB_TREE_NODE *terms;  /* subordinate glossary */
  struct NB_OBJECT *def;    /* term definition */
  } NB_Term;

extern NB_Term *termFree;
extern NB_Term *gloss;
extern struct TYPE *termType;
extern NB_Term *addrContext;
extern NB_Term *symContext;

void initTerm(NB_Stem *stem);
void termPrintName(NB_Term *term);
void nbTermAssign(NB_Term *term,NB_Object *new);
void nbTermShowItem(NB_Term *term);
void nbTermShowReport(NB_Term *term);
void termPrintGloss(NB_Term *gloss,NB_Type *type,int attr);
void termPrintGlossHome(NB_Term *gloss,NB_Type *type,int attr);

NB_Term *nbTermFind(NB_Term *term,char *identifier);
NB_Term *nbTermFindDown(NB_Term *term,char *identifier);
NB_Term *nbTermFindHere(NB_Term *term,NB_String *word);
NB_Term *nbTermFindDot(NB_Term *term,char **qualifier);


NB_Term *nbTermNew(NB_Term *context,char *ident,void *def);

void termUndef(NB_Term *term);
void termUndefAll(void);
void termResolve(NB_Term *term);

void nbTermName(char *name,size_t size,NB_Term *term,NB_Term *refContext);
//void termGetName(char *name,NB_Term *term,NB_Term *refContext);
void termPrintName(NB_Term *term);
void termPrintFullName(NB_Term *term);
void nbTermPrintLongName(NB_Term *term);

#endif  // NB_INTERNAL

// External API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTermCreate(nbCELL context,char *identifier,nbCELL definition);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTermLocate(nbCELL context,char *identifier);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTermLocateHere(nbCELL context,char *identifier);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTermGetDefinition(nbCELL context,nbCELL term);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbTermGetName(nbCELL context,nbCELL term);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTermSetDefinition(nbCELL context,nbCELL term,nbCELL definition);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTermPrintGloss(nbCELL context,nbCELL term);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbSetContext(nbCELL context);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern 
char *nbTermOptionStringTrueFalseUnknown(nbCELL context,char *name,char *defaultTrue,char *defaultFalse,char *defaultUnknown);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern 
char *nbTermOptionStringSilent(nbCELL context,char *name,char *defaultValue);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbTermOptionString(nbCELL context,char *name,char *defaultValue);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbTermOptionInteger(nbCELL context,char *name,int defaultValue);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern double nbTermOptionReal(nbCELL context,char *name,double defaultValue);

#endif
