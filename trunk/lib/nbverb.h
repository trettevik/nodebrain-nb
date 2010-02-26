/*
* Copyright (C) 1998-2009 The Boeing Company
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
* File:     nbverb.h 
* 
* Title:    Verb Header 
*
* Function:
*
*   This header defines routines that manage nodebrain verb objects.  The
*   verb object is not an extension of NB_OBJECT because we have not need to
*   manage these objects.
*
* See nbverb.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005/11/22 Ed Trettevik (Introduced in version 0.6.4)
*=============================================================================
*/
#ifndef _NB_VERB_H_
#define _NB_VERB_H_

#if defined(NB_INTERNAL)

#include <nbcell.h>

struct NB_VERB{
  char verb[12];              // verb
  int  authmask;              // authority mask
  int  flags;                 // flag bits
  char *syntax;               // syntax description
  void (*parse)(struct NB_CELL *context,char *verb,char *cursor);
  struct NB_VERB *lower;      // lower verb
  struct NB_VERB *higher;     // higher verb
  };

#define NB_VERB_LOCAL 1   // verb is interpreted locally - not sent to peers

void nbVerbPrint();
void nbVerbPrintAll();
int nbVerbDefine(
  struct NB_STEM *stem,
  char *verb,
  int authmask,
  int flags,
  void (*parse)(struct NB_CELL *context,char *verb,char *cursor),char *syntax);
struct NB_VERB *nbVerbBalance(struct NB_VERB *verbEntry,int n);
struct NB_VERB *nbVerbFind(struct NB_VERB *verbEntry,char *verb);

#endif // NB_INTERNAL

#endif
