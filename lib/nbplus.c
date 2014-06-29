/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbplus.c 
*
* Title:    NodeBrain "+:" Command Interpreter
*
* Function:
*
*   This file provides a customizable "+:" command interpreter as a simple
*   way to extend NodeBrain without building a skill module.  You can use
*   the NodeBrain C API within this file.  See online documentation at
*   http://www.nodebrain.org for a description of the API.
*
* WARNING:
*
*   This is deprecated and will be removed in a future release
*
* Description:
*
*   The nbPlus() function is called when a "+:" command is encountered. Here
*   is a simple "albert" command (whatever that means to you), and an "albert" 
*   command that fires as a rule action.     
*
*     define connie node;
*     connie:albert
*     connie. define r1 on(a=1 and b=2):+:albert
*
*   When the interpreter wants to execute the "albert" commands, it calls 
*   the nbPlus() function.
*
*     nbPlus(nbCell context,char *command,int mode);
*
*     context - handle for the "connie" context
*     command - pointer to null terminated command string "albert"
*     mode    - preferred logging option
*
*   You would implement your "albert" command interpreter in the nbPlus()
*   function in this file.  Then you would rebuild nb using the "make"
*   command without modifying Makefile. 
*
*   The nbPlus() command is the simplest way to extend the language for your
*   application.  Since you create a custom build of nb, it is wise to give
*   your program a different name than nb.
*    
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2005/04/10 Ed Trettevik (separated from nb.c in 0.6.2)
*=============================================================================
*/
#include <nb/nbi.h>

/*
*  Handle "+:" commands
*/
void nbPlus(nbCELL context,char *cursor,int cmdopt){

  /* API - replace the following code with your own code */

  if(strncmp(cursor,"nb ",3)==0){ /* parse your own command */

    /* This is how you pass a message to NodeBrain for output */

	nbLogMsg(context,2,'W',"Factory '+' command handler passing command to interpreter");

    /* This is how you call NodeBrain interpreter */

    nbCmd(context,cursor+3,1);  /* we elect to echo the modified command */

    }

  else nbLogMsg(context,1,'E',"Factory '+' command handler ignoring command.");
  }
