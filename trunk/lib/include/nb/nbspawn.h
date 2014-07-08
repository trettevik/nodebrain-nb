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
