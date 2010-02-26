/*
* Copyright (C) 2003-2010 The Boeing Company
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
* File:     nb_string.c
*
* Title:    Toy Node Module - Example
*
* Function:
*
*   This file is a node module example illustrating how you can
*   extend NodeBrain by adding support for new types of nodes.
* 
* Reference:
*
*   See "NodeBrain API Reference" for more information.
*
* Syntax:
*
*   Module Declaration:  (normally not required)
*
*     declare <module> module <skill_module>
*
*     declare string  module /usr/local/lib/nb_string.so;       [Linux and Solaris]
*     declare string  module nb_string.dll;               [Windows]
*
*   Skill Declaration:   (normally not required)
*
*     declare <skill> skill <module>.<symbol>[(args)][:text]
*
*     declare chrswap  skill string.chrswap;
*
*   Node Definition:
*
*     define <term> node <skill>[(args)][:text]
*
*     define chrswap node string.chrswap;
*
*   Cell Expression:
*
*     ... <node>[(args)] ...
*
*     define r1 on(chrswap(a,";,")="fred,bill"):...
*
*   Assertion:
*
*     assert <node>(args) ...
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2009-09-29 Ed Trettevik - initial prototype
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
*=============================================================================
*/
#include "config.h"
#include <nb.h>

#if defined(_WINDOWS)
BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved){
	switch(fdwReason){
	  case DLL_PROCESS_ATTACH: break;
	  case DLL_THREAD_ATTACH: break;
	  case DLL_THREAD_DETACH: break;
	  case DLL_PROCESS_DETACH: break;
	}
  return(TRUE);
  }
#endif

/*
*========================================================================
* chrsub() skill example:
*
* Description:
*
*   The chrsub skill substitutes characters within a string.
*
*       define chrsub node string.chrsub;
*       define x cell chrsub(a,";,");
*       define r1 on(x="abc,def,xyz");
*
*       assert a="abc,def;xyz";
*
*       Value of x is "abc,def,xyz" and r1 fires.
*
*   This skill provides only an Evaluate method, so it is not capable
*   of commands and assertions.
*
*========================================================================
*/

/* Evaluation Method */

nbCELL chrsubEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  nbCELL argCell1,argCell2;
  nbSET argSet;
  int i,type,len;
  char *strIn,*strSub,*strChr;
  char strBuf[NB_BUFSIZE];

  argSet=nbListOpen(context,arglist);
  if(argSet==NULL) return(NB_CELL_UNKNOWN);
  argCell1=nbListGetCellValue(context,&argSet);
  if(argCell1==NULL) return(argCell1);
  type=nbCellGetType(context,argCell1);
  if(type!=NB_TYPE_STRING){
    nbCellDrop(context,argCell1);
    return(argCell1);
    }
  strIn=nbCellGetString(context,argCell1);
  nbCellDrop(context,argCell1);
  argCell2=nbListGetCellValue(context,&argSet);
  if(argCell2==NULL) return(argCell1);
  type=nbCellGetType(context,argCell2);
  if(type!=NB_TYPE_STRING){
    nbCellDrop(context,argCell2);
    nbCellDrop(context,argCell1);
    return(argCell1);
    }
  strSub=nbCellGetString(context,argCell2);
  len=strlen(strSub);
  strcpy(strBuf,strIn);
  for(i=0;i<len-1;i+=2){
    strIn=strBuf;
    while((strChr=strchr(strIn,strSub[i]))!=NULL){
      *strChr=strSub[i+1];
      strIn=strChr;
      }
    }
  nbCellDrop(context,argCell1);
  nbCellDrop(context,argCell2);
  return(nbCellCreateString(context,strBuf));
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern
#endif
void *chrsubBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,chrsubEvaluate);
  return(NULL);
  }
