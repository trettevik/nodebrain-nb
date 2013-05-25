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
* File:     nbsource.h
*
* Title:    File Source Routine Header 
*
* Function:
*
*   This header defines routines for sourcing NodeBrain rule files.
*
* See nbsource.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010-11-07 Ed Trettevik - (Version 0.8.5 - split out from nbcmd.c)
* 2013-04-27 eat 0.8.15 Included option parameter in nbSource header
*=============================================================================
*/
#ifndef _NB_SOURCE_H_
#define _NB_SOURCE_H_

#if defined(NB_INTERNAL) 

int nbSourceTil(struct NB_CELL *context,FILE *file);
int nbSourceIgnoreTil(struct NB_CELL *context,FILE *file,char *buf,int til);
 
#endif // !NB_INTERNAL (external API)

void nbSource(nbCELL context,int option,char *cursor);

#endif 
