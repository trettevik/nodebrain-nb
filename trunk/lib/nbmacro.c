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
* File:     nbmacro.c
*
* Title:    NodeBrain Macro Routines
*
* Function:
*
*   This file provides routines that implement NodeBrain string macros.
*
* Synospis:
*
*   #include "nb.h"
*
*   void nbMacroInit();
*   NB_Macro *nbMacroParse(char *source);
*   char *nbMacroSub(NB_Term *context,char **cursorP);
*
* Description
*
*   A string macro converts a string of the form 
*
*        <macro_term>(<arg1>,<arg2>,...:<assertion>)
*
*   into a new string as specified in the macro definition.
*
*        define <term> macro(<parm1>,<parm2>,...:<default_assertion>):
*          <string_with_symbolic_substitution>
*
*   The symbolic substitution is specified as %%{<term>}, where <term>
*   is a position parameter <parmN> or a keyword parameter defined in
*   the default assertion.
*
*   nbMacroInit()
*
*        Initialize the macro object type.
*
*   nbMacroParse()
*
*        Parse a macro definition and create a macro object.
*        
*   nbMacroSub()
*
*        Convert a macro substitution string into a new string object.     
*  
* Return Codes:
*
*   Both nbMacroParse() and nbMacroSub() return NULL values if they encounter
*   an error.  Error messages are emitted, not returned.
* 
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-11-17 Ed Trettevik (original prototype version)
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-26 dtl Added sstrcpy for Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-10-17 eat 0.8.12 Checker updates
* 2012-12-31 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>

NB_Type  *nb_MacroType;  /* macro type description */
NB_Macro *nb_MacroFree;  /* free macro structure */

/*
*  Macro Functions
*/        
void nbMacroPrint(NB_Macro *macro){
  NB_Term *addrContextSave=addrContext;

  addrContext=macro->context;
  outPut("macro");
  if(macro->parms!=NULL || macro->defaults!=NULL){
    outPut("(");
    printMembers(macro->parms);
    if(macro->defaults!=NULL){
      outPut(":");
      printMembers(macro->defaults);
      }
    outPut(")");
    }
  outPut(":");
  printStringRaw(macro->string);
  addrContext=addrContextSave;
  } 

void nbMacroDestroy(NB_Macro *macro){
  dropMember(macro->parms);
  dropObject(macro->defaults);
  dropObject(macro->string);
  }  
    
/*
*  Initialize macro object type
*/
void nbMacroInit(NB_Stem *stem){
  nb_MacroType=newType(stem,"macro",NULL,0,nbMacroPrint,nbMacroDestroy);
  }

/*
*  Parse term list - this will be moved when used elsewhere
*/
NB_Link *nb_MacroBadMember=NULL;

NB_Link *nbMacroParseTermList(nbCELL context,char **source){
  NB_Object *object;
  NB_Link *member=NULL,*entry,**next;
  char symid=',',ident[256],*cursave;
  
  next=&member;
  while(symid==','){
    cursave=*source;
    symid=nbParseSymbol(ident,source);
    if(symid!='t'){
      if(member==NULL) return(NULL);
      outMsg(0,'E',"Expecting term at \"%s\".",cursave);
      dropMember(member);
      return(nb_MacroBadMember);
      }
    object=(NB_Object *)nbTermNew((NB_Term *)context,ident,nb_Unknown);
    if((entry=nb_LinkFree)==NULL) entry=nbAlloc(sizeof(NB_Link));
    else nb_LinkFree=entry->next;
    *next=entry;
    entry->next=NULL;
    next=&(entry->next);
    entry->object=grabObject(object);
    symid=nbParseSymbol(ident,source);
    }
  return(member);
  }
/*
*  Parse a macro definition
*
*    define <term> macro[(p1,p2,..:d1=v1,d2=v2,...)]:<string>
*/
NB_Macro *nbMacroParse(nbCELL context,char **source){
  char *cursor=*source;
  NB_Macro *macro;
  NB_Link *parms=NULL,*defaults=NULL;
  NB_Term *localContext;
  char buf[NB_BUFSIZE],*bufcur=buf,*bufend=buf+NB_BUFSIZE-3;

  localContext=grabObject(nbTermNew(NULL,"macro",nbNodeNew()));
  if(*cursor=='('){
    cursor++;
    if(nb_MacroBadMember==(parms=nbMacroParseTermList((nbCELL)localContext,&cursor))){
      dropObject(localContext);
      outMsg(0,'E',"Syntax error in parameter list.");
      return(NULL);
      }
    if(*cursor==':'){
      cursor++;
      defaults=grabObject(nbParseAssertion(localContext,(NB_Term *)context,&cursor));
      }
    if(*cursor!=')'){
      outMsg(0,'E',"Expecting ')' at \"%s\".",cursor);
      dropObject(localContext);
      dropMember(parms);
      dropMember(defaults);
      return(NULL);
      }
    cursor++;
    }
  while(*cursor==' ') cursor++;
  if(*cursor!=':'){
    outMsg(0,'E',"Expecting ':' at \"%s\".",cursor);
    dropObject(localContext);
    dropMember(parms);
    dropMember(defaults);
    return(NULL);
    }
  cursor++;
  while(*cursor==' ') cursor++;
  while(*cursor!=0){
    if(bufcur>bufend){
      outMsg(0,'E',"Macro definition may not exceed %d characters.",NB_BUFSIZE);
      dropObject(localContext);
      dropMember(parms);
      dropMember(defaults);
      return(NULL);
      }
    if(*cursor=='%' && *(cursor+1)=='%' && *(cursor+2)=='{'){
      *bufcur='%'; bufcur++;
      *bufcur='{'; bufcur++;
      cursor+=3;
      }
    else{
      *bufcur=*cursor;
      bufcur++;
      cursor++;
      }
    }
  *bufcur=0;
  macro=(NB_Macro *)newObject(nb_MacroType,(void **)&nb_MacroFree,sizeof(NB_Macro));
  macro->context=localContext;
  macro->parms=parms;
  macro->defaults=defaults;
  macro->string=grabObject(useString(buf));
  *source=cursor;
  return(macro);
  }

/*
*  Perform macro substitution
*
*  Returns the cursor address in the source string following the macro
*  reference.  This makes it possible to use macro substitution within
*  different constructs. It is the caller's responsibility to check for
*  strange stuff after a macro reference.
*/
// 2008-05-26 eat 0.7.0 modified to return substitution string and update cursor
char *nbMacroSub(nbCELL context,char **cursorP){
  char symid,ident[256],*source=*cursorP,*cursor=source;
  NB_Term *term;
  NB_Macro *macro;
  NB_Link *arg=NULL,*parm;
  NB_Link *assertion=NULL;
  
  symid=nbParseSymbol(ident,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting macro term at \"%s\".",*source);
    return(NULL);
    }
  if(NULL==(term=nbTermFind((NB_Term *)context,ident))){
    outMsg(0,'T',"Term \"%s\" not defined.",ident);
    return(NULL);
    }
  macro=(NB_Macro *)term->def;
  if(macro->object.type!=nb_MacroType){
    outMsg(0,'T',"Term \"%s\" not defined as macro.",ident);
    return(NULL);
    }
  if(macro->defaults!=NULL) assert(macro->defaults,0);
  if(*cursor=='('){
    cursor++;
    /* assign positional parameters the argument values */
    parm=macro->parms;
    arg=newMember((NB_Term *)context,&cursor);
    while(parm!=NULL && arg!=NULL){
      nbTermAssign((NB_Term *)parm->object,(NB_Object *)arg->object->value);
      parm=parm->next;
      arg=arg->next;
      }
    if(arg!=NULL){
      outMsg(0,'E',"Too many arguments to macro \"%s\".",ident);
      dropMember(arg);
      return(NULL);
      }
    dropMember(arg);
    if(parm!=NULL){
      outMsg(0,'E',"Not enough arguments to macro \"%s\".",ident);
      return(NULL);
      }
    /* assign keyword parameters */
    if(*cursor==':'){
      cursor++;
      assertion=nbParseAssertion(macro->context,(NB_Term *)context,&cursor);
      assert(assertion,0);
      }
    dropMember(assertion);
    if(*cursor!=')'){
      outMsg(0,'E',"Expecting ')' at \"%s\".",cursor);
      return(NULL);
      }
    cursor++;
    }
  else if(macro->parms!=NULL){
    outMsg(0,'E',"Arguments required for macro \"%s\".",ident);
    return(NULL);
    }
  *cursorP=cursor;
  //return(nbSymSource(macro->context,macro->string->value));
  cursor=macro->string->value;
  if(*cursor=='%' && *(cursor+1)==' ') return(nbSymCmd((nbCELL)macro->context,cursor,"%{}"));
  return(cursor);
  }

/*
*  Perform macro substitution and create a string object
*/
NB_String *nbMacroString(nbCELL context,char **cursorP){
  char *buf;
  buf=nbMacroSub(context,cursorP);
  if(buf==NULL) return(NULL);
  return(useString(buf));
  }

// Open file with O_CREAT flags, validate data before open, handle open error
// centralized data validation routines
/* 2012-12-31 eat - VID 4729-0.8.13-1  While this reduced the number of checker errors, it didn't change the issue
int openCreate(char *filename, int flags, mode_t mode){
  int file;
  if (filename!=NULL && *filename!=0)
    if ((file=open(filename,flags,mode)) != -1) return(file); //Success: return != -1
// handle error here if nee
  return(-1);
  }
*/

// Open file for read, validate data before open, handle open error
/* 2012-12-31 eat - VID 4738-0.8.13-1  While this reduced the number of check errors, it didn't change the issue
int openRead(char *filename, int flags){
  int file;
  if (filename!=NULL && *filename!=0)
    if ((file=open(filename,flags)) != -1) return(file); //Success: return != -1
  // handle error here if nee
  return(-1);
  }
*/

