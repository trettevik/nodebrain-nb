/*
* Copyright (C) 2011-2012 The Boeing Company
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
* File:     nbtext.h 
* 
* Title:    Object Header (prototype)
*
* Function:
*
*   This header defines routines that manage nodebrain text objects.
*
* See nbtext.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2011-11-05 Ed Trettevik - original prototype
*=============================================================================
*/
#ifndef _NBTEXT_H_
#define _NBTEXT_H_       /* never again */

#include <nbstem.h>

typedef struct NB_TEXT{
  struct NB_OBJECT object;    /* object header */
  char value[1];              /* length determined when created */
  } NB_Text;

extern NB_Type *textType;

void initText(NB_Stem *stem);
void printText(NB_Text *text);
void destroyText(NB_Text *text);
NB_Text *nbTextLoad(char *filename);
NB_Text *nbTextCreate(char *textStr);

#endif
