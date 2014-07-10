/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nb_set.c
*
* Title:    Set Skill Module
*
* Purpose:
*
*   This file is a NodeBrain skill module for storing and looking
*   up values in a binary tree structure managed by the nodebrain set API.
* 
* Reference:
*
*   See NodeBrain Library manual for more information.
*
* Syntax:
*
*   Node Definition:
*
*     define <node> node <skill>[(args)][:text]
*
*     define set node set;
*
*   Command:
*
*     <node>[(args)][:text]
*
*   Cell Expression:
*
*     ... <node>(args) ...
*
*   Assertion:
*
*     assert <node>(args),...
*     define r1 on(condition) <node>(args),...
*
* Description:
*   
*   A set node is similar to a single attribute tree node, but has
*   significant differences.
*
*   o It interprets a multiple attribute tuple as multiple references to set
*     membership.
*
*     define stuffSet node set;
*     assert stuffSet("abc",5,"def");
*     define setRule on(stuffSet("abc",5));
*
*     define stuffTree node tree;
*     stuffTree. assert ("abc"),(5),("def");
*     define treeRule on(stuffTree("abc") and stuffTree(5));
*   
*   o Unlike a tree, a set node does not support the assignment of values
*     to set members. The following is not supported.
*
*     assert stuffSet("abc")=5;   # [not supported]
*
*   o Unlike a tree, a set may not be ordered.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2013-12-30 Ed Trettevik - first prototype
*=============================================================================
*/
#include "config.h"
#include <nb/nb.h>

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

//===================================================================================================

/* Tree skill and node structures */

typedef struct{
  int    options;        // option flags
  NB_SetMember *root;    // root pointer for the set
  } Set;

#define SET_OPTION_TRACE       1  // use closed world assumption

typedef struct{
  char trace;                    /* trace option */
  } SetSkill;

//===========================================================================================================
/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*/
static void *setConstruct(nbCELL context,SetSkill *skillHandle,nbCELL arglist,char *text){
  int options=0;
  nbCELL found=NULL;
  nbCELL notfound=NULL;
  nbCELL cell;
  Set *set;
  char *cursor=text,*delim,ident[256];
  int len;

  if(skillHandle!=NULL && skillHandle->trace) nbLogMsg(context,0,'T',"sample.setConstruct() called");
  while(*cursor==' ') cursor++;
  while(*cursor!=0 && *cursor!=';'){
    delim=cursor;
    while(*delim>='a' && *delim<='z') delim++;
    len=delim-cursor;
    if(len>sizeof(ident)-1){
      nbLogMsg(context,0,'E',"Option not recognized at \"%s\".",cursor);
      return(NULL);
      }
    strncpy(ident,cursor,len);
    *(ident+len)=0;
    cursor=delim; 
    while(*cursor==' ') cursor++;
    if(strcmp(ident,"trace")==0) options|=SET_OPTION_TRACE;
    else if(strcmp(ident,"found")==0 || strcmp(ident,"notfound")==0){
      if(*cursor!='='){
        nbLogMsg(context,0,'E',"Expecting '=' at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      cell=nbCellParse(context,&cursor);
      if(cell==NULL){
        nbLogMsg(context,0,'E',"Syntax error in cell expression.");
        return(NULL);
        }
      if(strcmp(ident,"found")==0) found=cell;
      else notfound=cell;
      }
    else{
      nbLogMsg(context,0,'E',"Option not recognized at \"%s\".",cursor);
      return(NULL);
      }
    while(*cursor==' ') cursor++;
    if(*cursor==','){
      cursor++;
      while(*cursor==' ') cursor++;
      }
    else if(*cursor!=0 && *cursor!=';'){
      nbLogMsg(context,0,'E',"Expecting ',' ';' or end of line at \"%s\".",cursor);
      return(NULL);
      }
    }
  if(notfound==NULL) notfound=NB_CELL_UNKNOWN;
  if(found==NULL) found=notfound;
  set=(Set *)nbAlloc(sizeof(Set));
  set->options=options;
  set->root=NULL;
  return(set);
  }

/*
*  assert() method
*
*    assert <node>[(args)][=<value>]
*
*    assert table("a",2,"hello")=5;   # set value to 5
*    assert table("a",2,"hello");     # set value to 1
*    assert !table("a",2,"hello");    # set value to 0
*    assert ?table("a",2,"hello");    # remove from table
*    assert table("a",2,"hello")=??   # remove from table
*
*/
static int setAssert(nbCELL context,void *skillHandle,Set *set,nbCELL arglist,nbCELL value){
  NB_SetMember *node=NULL,*parent,**nodeP;
  nbCELL argCell;
  nbSET  argSet;

  if(arglist==NULL) return(0); // perhaps we should return the value of the set itself
  argSet=nbListOpen(context,arglist);
/* Implement set empty first 
  if(value==NB_CELL_UNKNOWN || value==NB_CELL_FALSE){
    if(argSet==NULL && set->root!=NULL) set->root=setEmpty(set->root);
    return(0);
    }
*/
  if(argSet==NULL) return(0);
  while((argCell=nbListGetCellValue(context,&argSet))!=NULL){
    nodeP=&set->root;
    NB_SET_LOCATE_MEMBER(argCell,node,parent,nodeP)
    if(value==NB_CELL_UNKNOWN || value==NB_CELL_FALSE){
      nbCellDrop(context,argCell);
      if(node!=NULL){
        nbCellDrop(context,node->member);
        nbSetRemove((NB_SetNode *)&set->root,(NB_SetNode *)node);
        nbFree(node,sizeof(NB_SetMember));
        }
      }
    else{
      if(node==NULL){
        node=nbAlloc(sizeof(NB_SetMember));
        memset(node,0,sizeof(NB_SetMember));
        nbSetInsert((NB_SetNode *)&set->root,(NB_SetNode *)parent,(NB_SetNode **)nodeP,(NB_SetNode *)node);
        node->member=(void *)argCell;
        }
      else nbCellDrop(context,argCell);
      }
    }
  nbLogFlush(context);
  return(0);
  }

/*
*  evaluate() method
*
*    ... <node>[(<args>)] ...
*
*    define r1 on(table("a",2,"hello")=4);
*
*/
static nbCELL setEvaluate(nbCELL context,SetSkill *skillHandle,Set *set,nbCELL arglist){
  nbCELL    argCell;
  nbSET     argSet;
  NB_SetMember  *node=NULL;

  if(skillHandle->trace || set->options&SET_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_set::setEvaluate()");
    nbLogPut(context,"set");
    if(arglist!=NULL) nbCellShow(context,arglist);
    nbLogPut(context,"\n");
    }
  if(arglist==NULL){  // request for set value
    return(NB_CELL_UNKNOWN);  // for now, let it be Unknown
    }
  argSet=nbListOpen(context,arglist);
  if(argSet==NULL) return(NB_CELL_FALSE); // set() returns default value
  argCell=nbListGetCellValue(context,&argSet);
  while(argCell!=NULL && argSet!=NULL){
    node=set->root;
    NB_SET_FIND_MEMBER(argCell,node)
    if(node==NULL){
      nbCellDrop(context,argCell);
      return(NB_CELL_FALSE);
      }
    nbCellDrop(context,argCell);
    argCell=nbListGetCellValue(context,&argSet);
    }
  return(NB_CELL_TRUE);
  }

/*
*  show() method
*
*    show <node>;
*
*    show table;
*/
static int setShow(nbCELL context,void *skillHandle,Set *set,int option){
  NB_SetIterator iterator;
  NB_SetNode *setNode;
  NB_SetMember *setMember;

  if(option!=NB_SHOW_REPORT) return(0);
  NB_SET_ITERATE(iterator,setNode,set->root){
    setMember=(NB_SetMember *)setNode;
    nbCellShow(context,(nbCELL)setMember->member);
    nbLogPut(context,"\n");
    NB_SET_ITERATE_NEXT(iterator,setNode) 
    }
  return(0);
  }

static int setGetIdent(char **cursorP,char *ident,int size){
  char *cursor=*cursorP,*delim;
  int len;

  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim>='a' && *delim<='z') delim++;
  len=delim-cursor;
  if(len>size-1) return(-1);
  strncpy(ident,cursor,len);
  *(ident+len)=0;
  *cursorP=delim;
  return(len);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *setCommand(nbCELL context,SetSkill *skillHandle,Set *set,nbCELL arglist,char *text){
  char *cursor=text,ident[512];
  int len;

  if(skillHandle->trace || set->options&SET_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_set:setCommand() text=[%s]\n",text);
    }
  len=setGetIdent(&cursor,ident,sizeof(ident));
  if(len<0){
    nbLogMsg(context,0,'E',"Verb not recognized at \"%s\".",cursor);
    return(0);
    }
  while(*cursor==' ') cursor++;
  if(strcmp(ident,"trace")==0){
    len=setGetIdent(&cursor,ident,sizeof(ident));
    if(len==0 || strcmp(ident,"on")) set->options|=SET_OPTION_TRACE;
    else if(strcmp(ident,"off")) set->options&=0xffffffff-SET_OPTION_TRACE;
    }
  else nbLogMsg(context,0,'E',"Verb \"%s\" not recognized.",ident);
  return(0);
  }

/*
*  skill initialization method
*
*    declare <term> skill <module>.<symbol>[(<args>)][:<text>]
*
*    declare table node tree.tree;
*/
#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *setBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  SetSkill *skillHandle;
  char *cursor=text;
  //nbCELL facet; // 2013-12-07 eat - experimenting with facets

  skillHandle=(SetSkill *)nbAlloc(sizeof(SetSkill));
  skillHandle->trace=0;
  while(*cursor==' ') cursor++;
  while(*cursor!=0 && *cursor!=';'){
    if(strncmp(cursor,"trace",5)==0){
      skillHandle->trace=1;
      cursor+=5;
      }
    else{
      nbLogMsg(context,0,'T',"Option not recognized at \"%s\".",cursor);
      nbFree(skillHandle,sizeof(SetSkill));
      return(NULL);
      }
    while(*cursor==' ' || *cursor==',') cursor++;
    }
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,setConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,setAssert);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,setEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,setShow);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,setCommand);

  return(skillHandle);
  }
