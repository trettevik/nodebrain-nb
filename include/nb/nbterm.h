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
* 2014-01-26 eat 0.9.00 Switched glossary from tree to hash
*=============================================================================
*/
#ifndef _NB_TERM_H_
#define _NB_TERM_H_

#include <nb/nbcell.h>
#include <nb/nbstem.h>

#if defined(NB_INTERNAL)

/* For type identification, class should be phased out.  The term.def->type
*  provides the type of object associated with a term.
*
*  For inheritance, class has been replaced by NB_NODE.REF
*/
typedef struct NB_TERM{
  NB_Cell cell;                  /* cell object header */
  // 2014-01-26 eat - returning to a hash structure for glossaries
  // the next four entries are a tree node
  // if this experiment works, we need to replace with
  // struct NB_TreeNode   and reference word ast treeNode.key
  //void *left;
  //void *right;
  //signed int balance;
  NB_String *word;               /* word defined within the context */
  // 2014-01-26 eat - after converting to a hash - consider moving ot NB_Node
  struct NB_TERM   *context;     /* parent context term */
  //struct NB_TREE_NODE *terms;  /* subordinate glossary */
  NB_Hash *gloss;                // subordinate glossary of terms
  NB_Object *def;                // term definition
  } NB_Term;

extern NB_Term *termFree;
extern NB_Term *rootGloss;
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


NB_Term *nbTermNew(NB_Term *context,char *ident,void *def,int option);

void nbTermUndefine(NB_Term *term);
void termUndefAll(void);
void termResolve(NB_Term *term);

void nbTermName(NB_Term *context,NB_Term *term,char *name,size_t size);
void termPrintName(NB_Term *term);
void termPrintFullName(NB_Term *term);
void nbTermPrintLongName(NB_Term *term);
int NbTermGetTermCellArray(NB_Term *context,nbCELL cell[],int cells);
int NbTermGetTermNameString(NB_Term *context,char **bufP,int size);
int NbTermGetTermValueString(NB_Term *context,char **bufP,int size);
int NbTermGetTermFormulaString(NB_Term *context,char **bufP,int size);

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
