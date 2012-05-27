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
* File:     nbtranslator.h
*
* Title:    Text Translator Header (prototype)
*
* Function:
*
*   This header defines routines to manage translation of text lines or streams
*   of text lines into NodeBrain commands.
*
* See nbtranslator.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-16 eat 0.5.1  Split nbxtr.c off nbxtr.h for make file
* 2003-03-17 eat 0.5.2  Conditioning out Windows here instead of at include
* 2005-04-04 eat 0.6.2  Renamed and modified for use in the API for modules
* 2005-04-08 eat 0.6.2  API function definitions moved to nbapi.h
* 2006-05-09 eat 0.6.6  Included symbolic context
* 2008-10-04 eat 0.7.2  Removed symbolic context
* 2008-10-06 eat 0.7.2  Modified to use NodeBrain's API for regular expressions
* 2008-10-14 eat 0.7.2  Modified to make projection a NodeBrain object
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_TRANSLATOR_H_
#define _NB_TRANSLATOR_H_

#if defined(NB_INTERNAL)

typedef struct NB_PROJECTION{
  struct NB_OBJECT object;      // object header
  short length;                 // length of projection code
  char  code[0];                // projection byte code of length specified 
  } NB_Projection;

typedef struct NB_TRANSLATOR{
  struct NB_OBJECT object;
  struct STRING    *filename;   // file containing source code
  struct NB_XI      *xi;        // first translation instruction
  int              depth;       // maximum nesting of expressions
  } NB_Translator;

struct NB_XI{
  // next four fields must conform to NB_TreeNode structure
  struct NB_XI *left;        // left entry in this tree */
  struct NB_XI *right;       // right entry in this tree */
  signed int   balance;     // AVL balance code (-1 left tall, 0 - balanced, +1 right tall)
  union{
    struct NB_CELL *cell;        // common name */
    struct STRING *label;        // label - NB_XI_OPER_LABEL 
    struct STRING *string;       // string for full match  - NB_XI_OPER_STRING
    struct REGEXP *re;           // regex for match - NB_XI_OPER_REGEX
    NB_Projection *projection;   // projection - NB_XI_OPER_COMMAND or NB_XI_OPER_TRANSFORM
    } item;
  unsigned char oper;     // Operation code---see NB_XI_OPER_*
  unsigned char flag;     // Flag bits---see NB_XI_FLAG_*
  NB_TreeNode *tree;
  struct NB_XI *next;
  struct NB_XI *nest;    // nested commands
  };

// Operation codes  
#define NB_XI_OPER_FILE        0  // file - noop
#define NB_XI_OPER_LABEL       1  // label - noop
#define NB_XI_OPER_STRING      2  // string condition
#define NB_XI_OPER_REGEX       3  // regex condition 
#define NB_XI_OPER_COMMAND     4  // command projection
#define NB_XI_OPER_TRANSFORM   5  // text projection
#define NB_XI_OPER_SEARCH      6  // binary tree search
#define NB_XI_OPER_STATIC    0x07 // Mask to get opcode without dynamic bits
// Flag bits and masks
#define NB_XI_OPER_DISABLED  0x80 // Set when command is disabled
#define NB_XI_OPER_ALL       0xff // Mask to get opcode without dynamic bits

#define NB_XI_FLAG_FAILTHRU    1  // Continue from the next instruction if no match
#define NB_XI_FLAG_MATCHTHRU   2  // Continue from the next instruction even after match
#define NB_XI_FLAG_STATIC      3  // Mast to get flag without dynamic bits

#define NB_XI_FLAG_REUSE      16  // Reuse request  '^'
#define NB_XI_FLAG_INHERIT    (NB_XI_FLAG_REUSE)
#define NB_XI_FLAG_AFTER      32  // Insert after   '>'
#define NB_XI_FLAG_BEFORE     64  // Insert before  '<'
#define NB_XI_FLAG_NEW        (NB_XI_FLAG_AFTER|NB_XI_FLAG_BEFORE)
#define NB_XI_FLAG_REPLACE   128  // Replace        '='
#define NB_XI_FLAG_MODIFY     (NB_XI_FLAG_AFTER|NB_XI_FLAG_BEFORE|NB_XI_FLAG_REPLACE)
#define NB_XI_FLAG_PARSE      (NB_XI_FLAG_DELETE|NB_XI_FLAG_REUSE|NB_XI_FLAG_AFTER|NB_XI_FLAG_BEFORE|NB_XI_FLAG_REPLACE)
#define NB_XI_FLAG_ALL      0xff

extern struct TYPE *nb_TranslatorType;

void nbTranslatorInit(NB_Stem *stem);
void nbProjectionShowAll(void);

#endif // NB_INTERNAL

//***********************************
// External API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern nbCELL nbTranslatorCompile(nbCELL context,char *file);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbTranslatorDo(nbCELL context,nbCELL translator,char *text);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbTranslatorRefresh(nbCELL context,nbCELL translator);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTranslatorExecute(nbCELL context,nbCELL translator,char *source);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTranslatorExecuteFile(nbCELL context,nbCELL translator,char *filename);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTranslatorShow(nbCELL translatorCell);

#endif
