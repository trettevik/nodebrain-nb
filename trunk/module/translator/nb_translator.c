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
*
* Name:   nb_translator.c
*
* Title:  Text Translator   
*
* Function:
*
*   This program is a NodeBrain skill module for translating text lines
*   into NodeBrain commands.
*   
* Synopsis:
*
*   define <node> node translator("<file>")[:<options>];
*
*       <file>     -  File containing translation rules. 
*                     (This is normally an *.nbx file name.)
*                   
*       <options>  -  Options to control the log output 
*
*                     trace   - display every line of text asserted
*                     silent  - don't echo generated NodeBrain commands
*
*   define <term> on(<condition>) <node>(<argList>);
*   assert <node>(<argList);
*
*       <argList>  - String arguments are translated
*
*   <node>("translate"):<filename>
*
*       <file>     - File is translated
*
*
* Example:
*
*   define mytran node translator("mytran.nbx");  
*   assert mytran("this is line 1","this is line 2");
*   mytran:this is line 3
*   mytran("translate"):myfile
*
* Description:
*
*   This is an adaptation of NodeBrain's built-in text translation
*   capability.  Here we are really just making it available for use
*   in an assertion or node command syntax.
*
*   This module accepts assertions and commands only.  Ther is no support
*   for evaluation, so the following syntax will produce an error message
*   on each evaluation attempt. 
*
*     define r1 on(<term>(<cell_expression>)) ...
*
*===========================================================================
* NodeBrain API Functions Used:
*
*   Here's an outline of the API functions used by this module.  Refer to
*   the online NodeBrain User's Guide for a description of the NodeBrain
*   Skill Module API.  However, you can probably figure it out by
*   the example of this module using this outline as a starting point.
*   
*   Inteface to NodeBrain skill modules
*
*     nbSkillSetMethod()  - Used to tell NodeBrain about our methods.  
*
*       translatorConstruct()  - construct a translator node
*       translatorAssert()     - handle assertions
*       translatorCommand()    - handle a custom command
*       translatorDestroy()    - clean up when the node is destroyed
*
*   Interface to NodeBrain objects (primarily used in translatorConstruct)
*
*     nbCellCreate()      - Create a new cell
*     nbCellGetType()     - Identify the type of a cell
*     nbCellGetString()   - Get a C string from a string cell
*     nbCellGetReal()     - Get a C double value from a real cell
*     nbCellDrop()        - Release a cell (decrement use count)
*
*   Interface to NodeBrain actions
*
*     nbLogMsg()		      - write a message to the log
*     nbCmd()             - Issue a NodeBrain command
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2005/04/04 Ed Trettevik - original prototype version 0.6.2
* 2005/04/09 eat 0.6.2  adapted to API changes
* 2005/05/14 eat 0.6.3  translateBind() updated for moduleHandle
* 2007/06/26 eat 0.6.8  updated to satisfy original intent 
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-10-17 eat 0.8.12 Checker updates
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>

/*
*  The following structure is created once at bind time
*/
typedef struct NB_MOD_TRANSLATOR_SKILL{
                                 // String cells are used as command identifiers
  nbCELL translateStr;           // "translate"
  nbCELL doStr;                  // "do"
  nbCELL refreshStr;             // "refresh"
  } NB_MOD_TranslatorSkill;

/*
*  The following structure is created by the skill module's "construct"
*  function (translatorConstruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_TRANSLATOR{           /* translator node descriptor */
  nbCELL *translator;              /* translator object */
  char filename[512];              /* translation rule file name */
  unsigned char  trace;            /* trace option */
  unsigned char  echo;             /* echo option */
  } NB_MOD_Translator;

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.  For example, translateRead() is an
*  example of a method that would only be used by a module that "listens".
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*
*    <args> - File name of translation rules (*.nbx) 
*    <text> - flag keywords
*               trace   - display input packets
*               silent  - don't echo generated NodeBrain commands 
*
*    define translate node translate("syslog.nbx");
*/
void *translatorConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Translator *translate;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char filename[512];
  int trace=0,echo=1;
  int type,len;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_STRING){
      str=nbCellGetString(context,cell);
      len=strlen(str);
      if(len>sizeof(filename)-1){
        nbLogMsg(context,0,'E',"Translation file name may not be greater than %d",sizeof(filename)-1);
        nbCellDrop(context,cell);
        return(NULL);
        }
      strncpy(filename,str,len);
      *(filename+len)=0; 
      nbCellDrop(context,cell);
      }
    else{
      nbLogMsg(context,0,'E',"Expecting translation rules (\"filename\") as argument list");
      return(NULL);
      }
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'E',"Unexpected argument - ignoring all but first argument");
      return(NULL);
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor);
    saveDelim=*delim;
    *delim=0;
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    *delim=saveDelim;
    cursor=delim;
    if(*cursor==',') cursor++;
    while(*cursor==' ') cursor++;
    }
  translate=nbAlloc(sizeof(NB_MOD_Translator));
  strcpy(translate->filename,filename);
  translate->trace=trace;
  translate->echo=echo;
  translate->translator=nbTranslatorCompile(context,0,translate->filename);
  if(translate->translator==NULL){
    nbLogMsg(context,0,'E',"Unable to load translator");
    return(NULL);
    }
  return(translate);
  }

/*
*  assert method
* 
*    disable <node>
*
*/
int translatorAssert(nbCELL context,void *skillHandle,NB_MOD_Translator *translate,nbCELL arglist){
  nbCELL cell=NULL;
  nbSET argSet;
  int type;
  char *text;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  while(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_STRING){
      text=nbCellGetString(context,cell);
      if(*text!=0) nbTranslatorExecute(context,translate->translator,text);
      }
    cell=nbListGetCellValue(context,&argSet);
    }
  return(0);
  }

/*
*  evaluate() method
*
*    ... <node>[(<args>)] ...
*
*    define c1 cell translator("str1","str2",...);
*
*    Each argument string is passed to the translator until a value other than
*    "Unknown" is returned.  The value of the cell is the first "known" value,
*    or "Unknown" if not value matches.
*/
static nbCELL translatorEvaluate(nbCELL context,void *skillHandle,NB_MOD_Translator *translate,nbCELL arglist){
  nbCELL cell=NULL,value=NB_CELL_UNKNOWN;
  nbSET argSet;
  int type;
  char *text;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  while(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_STRING){
      text=nbCellGetString(context,cell);
      if(*text!=0){
        value=nbTranslatorExecute(context,translate->translator,text);
        if(value!=NB_CELL_UNKNOWN) return(value);
        }
      }
    cell=nbListGetCellValue(context,&argSet);
    }
  return(value);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*/
int translatorCommand(nbCELL context,NB_MOD_TranslatorSkill *skillHandle,NB_MOD_Translator *translate,nbCELL arglist,char *text){
  nbCELL cell=NULL;
  nbSET argSet;
//  int type;
  char *filename;

  if(translate==NULL){
    nbLogMsg(context,0,'E',"Translator was not loaded---see message at node definition.");
    return(1);
    }
  if(translate->trace){
    nbLogMsg(context,0,'T',"translatorCommand() text=[%s]\n",text);
    }
  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){  //   Translate text if no arguments specified
    if(text!=NULL && *text) nbTranslatorExecute(context,translate->translator,text);
    }
  else if(cell==skillHandle->translateStr){
    if(text==NULL || *text==0){
      nbLogMsg(context,0,'E',"Expecting file name as text argument");
      return(1);
      }
    else{
      filename=text;
      while(*filename==' ') filename++;
      nbTranslatorExecuteFile(context,translate->translator,filename);
      }
    } 
  else if(cell==skillHandle->refreshStr){
    nbTranslatorRefresh(context,translate->translator);
    }
  else if(cell==skillHandle->doStr){
    nbTranslatorDo(context,translate->translator,text);
    nbCellPublish(context); // publish update to re-evaluate cell expressions
    nbRuleReact();
    }
  else{
    nbLogMsg(context,0,'E',"Command not recognized");
    return(1);
    }
  while((cell=nbListGetCellValue(context,&argSet))!=NULL){
    nbLogMsg(context,0,'W',"Extra argument ignored: %s",nbCellGetString(context,cell));
    nbCellDrop(context,cell);
    }
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
int translatorDestroy(nbCELL context,void *skillHandle,NB_MOD_Translator *translate){
  nbLogMsg(context,0,'T',"translatorDestroy called");
  nbFree(translate,sizeof(NB_MOD_Translator));
  return(0);
  }

int translatorShow(nbCELL context,void *skillHandle,NB_MOD_Translator *translate,int option){
  if(option!=NB_SHOW_REPORT) return(0);
  //nbLogPut(context,"\n");
  if(translate && translate->translator!=NULL) nbTranslatorShow(translate->translator);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *translatorBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  struct NB_MOD_TRANSLATOR_SKILL *skillHandle;
  skillHandle=nbAlloc(sizeof(struct NB_MOD_TRANSLATOR_SKILL));
  skillHandle->doStr=nbCellCreateString(context,"do");
  skillHandle->refreshStr=nbCellCreateString(context,"refresh");
  skillHandle->translateStr=nbCellCreateString(context,"translate");

  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,translatorConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,translatorAssert);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,translatorEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,translatorShow);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,translatorCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,translatorDestroy);
  return(skillHandle);
  }
