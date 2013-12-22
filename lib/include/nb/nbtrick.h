/*
* Copyright (C) 1998-2013 The Boeing Company
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
* File:     nbtrick.h
*
* Title:    Trick Cell Header
*
* Function:
*
*   This header defines routines that manage nodebrain trick cells.
*
* See nbtrick.c for more information.
*============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2013-12-09 Ed Trettevik (original prototype introduced in 0.9.0)
*============================================================================
*/
#ifndef _NB_TRICK_H_
#define _NB_TRICK_H_

#include <nb/nbstem.h>

/* 
* Relational Equal Operator Trick Object 
*/
typedef struct NB_TRICK_REL_EQ{ // trick object
  struct NB_CELL cell;          // object header
  struct NB_TREE_NODE *sub;     // subscriber tree
  struct NB_CELL   *pub;        // publishing cell
  struct NB_CELL   *trueCell;   // NULL or the one true RelEq cell with matching constant
                                // or nb_Unknown which means all subscribers have Unknown value
  } NB_TrickRelEq;

// Functions

void initTrick(NB_Stem *stem);
struct TYPE *nb_TypeTrickRelEq;
void nbTrickRelEqEnable(nbCELL pub,struct COND *cond);
void nbTrickRelEqDisable(nbCELL pub,struct COND *cond);

#endif
