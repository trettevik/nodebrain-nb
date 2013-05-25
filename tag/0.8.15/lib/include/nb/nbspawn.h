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
* File:     nbspawn.h
*
* Title:    Header for Skull and Shell Command Routines
*
* Function:
*
*   This header defines the functions used to spawn child processes for
*   of skull and shell commands.
*
* See nbspawn.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/10/08 Ed Trettevik (introduced in 0.4.1 A6)
* 2006/03/26 eat 0.6.4  spawnExec and spawnSystem merged into nbSpawnChild
* 2006/03/26 eat 0.6.4  spawnSkull replaced by nbSpawnSkull
*=============================================================================
*/

//#if defined(WIN32)
//extern HANDLE nbpTimer;
//extern LARGE_INTEGER alarmTime;
//#endif

int nbSpawnChild(NB_Cell *context,int options,char *command);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbSpawnSkull(NB_Cell *context,char *oar,char *command);