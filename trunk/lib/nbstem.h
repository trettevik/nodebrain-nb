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
* File:     nbstem.h
*
* Title:    NodeBrain Stem Cell Header
*
* Function:
*
*   This header defines the NodeBrain stem cell which "controls" a NodeBrain
*   environment.  This cell contains options and pointers to collections of
*   other cells.
*
* Description:
*
*   The plan is to transition away from global variables so we can have 
*   multiple instances of NodeBrain running concurrently.  We'll gradually
*   move global variables into the stem cell.  In many cases this will mean
*   we have to add a context or stem cell parameter to functions that need
*   access to the stem cell.  As long as a non-null parameter is an object,
*   we can access the stem cell via the NB_Type structure associated with
*   the object.
*
*      object->type->stem
*      cell->object.type->stem
*      term->cell.object.type->stem 
*
*   In some cases it may be more efficent to pass the stem cell as a 
*   parameter.  In any case, all functions that reference global variables
*   will need access to the stem cell as we move the variables into the
*   stem cell.
*
*   Ultimately it may be best to use the following parameter scheme.
*
*      nbSomething(NB_Stem *stem, ...);
*      nbSomething(NB_Stem *stem, NB_Cell *context, ... );
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2005/04/11 Ed Trettevik (split out in 0.6.2)
* 2005/12/30 eat 0.6.4  childmode removed - see nb_opt_servant
* 2008/03/24 eat 0.7.0  Removed parentChannel
*============================================================================
*/
#ifndef _NB_STEM_H_
#define _NB_STEM_H_

#if defined(NB_INTERNAL)

#include <nbcell.h>

typedef struct NB_STEM{
  void *type;                     // reserved for stem cell type pointer
  //struct CHANNEL *parentChannel;  // channel for communicating with parent process
  int exitcode;                   // exit code to use when we terminate
  struct NB_Todo *todo;           // todo list (commands to execute)
  struct NB_VERB *verbTree;       // verb definition tree
  int verbCount;                  // number of verbs defined
  } NB_Stem;

extern char *nb_cmd_prefix;
extern char *nb_cmd_prompt;
#define NB_CMD_PROMPT_LEN 1024    // Length of prompt and prefix buffers

/* 
*  Stem related function definitions
*/
void stdPrint(char *buffer);
void logPrint(char *buffer);
void logPrintNl(char *buffer);
//void clientPrint(char *buffer);

#endif // NB_INTERNAL

//****************************************
// External API

#if defined(WIN32)
__declspec(dllexport)
#endif
extern nbCELL nbStart(int argc,char *argv[]);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbServe(nbCELL context,int argc,char *argv[]);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbStop(nbCELL context);

//===============================================

#if defined(WIN32)
__declspec(dllexport)
#endif
extern char *nbGetUserDir();

#endif
