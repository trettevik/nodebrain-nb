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
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbverb.h 
*
* Title:    Verb Object Methods
*
* Function:
*
*   This file provides methods for NodeBrain VERB objects used internally
*   to parse commands.  The VERB type extends the OBJECT type defined in
*   nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initVerb();
*   struct VERB *newVerb();
*   void printVerb();
*   void printVerbAll();
* 
* Description
*
*   A verb object represents a verb within the NodeBrain language.  Verbs
*   are defined at initialization time only---verbs are never destroyed and
*   the language does not provide for user defined verbs.  The language is
*   extended only via node module commands.
*  
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005-11-22 Ed Trettevik (original prototype version introduced in 0.6.4)
* 2010-06-19 eat 0.8.2  Making verbs objects so modules can contribute extensions
*            By making verbs objects we can manage them via the API in a way 
*            consistent with other types of objects.
* 2012-01-16 dtl        Checker updates.
* 2013-01-11 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *nb_verbType;

//int nbVerbDeclare(nbCELL context,char *ident,int authmask,int flags,void (*parse)(struct NB_CELL *context,char *verb,char *cursor),char *syntax){
int nbVerbDeclare(nbCELL context,char *ident,int authmask,int flags,void *handle,int (*parse)(struct NB_CELL *context,void *handle,char *verb,char *cursor),char *syntax){
  struct NB_VERB *verb;
  
  verb=(struct NB_VERB *)newObject(nb_verbType,NULL,sizeof(struct NB_VERB));
  //strcpy(verb->verb,ident);   // depend on caller not to pass verb over 15 characters
  verb->authmask=authmask;
  verb->flags=flags;
  verb->handle=handle;
  verb->parse=parse;
  verb->syntax=syntax;
  verb->term=nbTermNew(context->object.type->stem->verbs,ident,verb);
  if(trace) outMsg(0,'T',"verb created - %s\n",ident);
  return(0);
  }

void nbVerbLoad(nbCELL context,char *ident){
  char *cursor,modName[256],msg[NB_MSGSIZE];
  size_t len;

  cursor=strchr(ident,'.');
  if(!cursor){
    outMsg(0,'E',"Expecting '.' in identifier %s",ident);
    return;
    }
  len=cursor-ident;
  if(len>=sizeof(modName)){
    outMsg(0,'E',"Module must not exceed %d characters in identifier %s",sizeof(modName)-1,ident);
    return;
    }
  strncpy(modName,ident,len); 
  *(modName+len)=0;
  nbModuleBind(context,modName,msg,sizeof(msg));
  }

struct NB_VERB *nbVerbFind(nbCELL context,char *ident){
  struct NB_STEM *stem=context->object.type->stem;
  struct NB_TERM *term;

  term=nbTermFindDown(stem->verbs,ident);
  if(!term){
    nbVerbLoad(context,ident);
    term=nbTermFindDown(stem->verbs,ident);
    if(!term) return(NULL);
    }
  return((struct NB_VERB *)term->def);
  }

/**********************************************************************
* Object Management Methods
**********************************************************************/
void nbVerbPrint(struct NB_VERB *verb){
  outPut("verb ::= %s",verb->syntax);
  }

//void nbVerbPrintAll(struct NB_VERB *verb){
void nbVerbPrintAll(nbCELL context){
  struct NB_STEM *stem;
  outPut("Command Syntax:\n");
  outPut(
    " <command> ::= [<context>. ]<command> |\n"
    "               <context>:<node_command> |\n"
    "               <context>(<list>):<node_command> |\n"
    "               #<comment>\n"
    "               ^<stdout_message>\n"
    "               -[|][:]<shell_command>\n"
    "               =[|][:]<shell_command>\n"
    "               {<rule>}\n"
    "               ?<cell>\n"
    "               (<option>[,...])<command>\n"
    "               `<assertion>\n"
    " <context> ::= <term>      # defined as a node\n"
    " <term>    ::= <ident>[.<term>]\n"
    " <ident>   ::= <alpha>[<alphanumerics>]\n"
    "-------------------------------------------------\n");
  outPut("Verb Table:\n");
  stem=context->object.type->stem;
  nbTermPrintGloss((nbCELL)stem->verbs,(nbCELL)stem->verbs);
  }

void nbVerbDestroy(struct NB_VERB *verb){
  nbFree(verb,sizeof(struct NB_VERB));
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbVerbInit(NB_Stem *stem){
  nb_verbType=newType(stem,"verb",NULL,0,nbVerbPrint,nbVerbDestroy);
  nb_verbType->apicelltype=NB_TYPE_VERB;
  stem->verbs=nbTermNew(NULL,"verb",useString("verb"));
  }
