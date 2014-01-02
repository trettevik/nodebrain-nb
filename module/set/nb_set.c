/*
* Copyright (C) 2003-2014 The Boeing Company
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
*   See NodeBrain API Reference for more information.
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
/*
typedef struct BTREE_NODE{
  NB_TreeNode bnode;           // binary tree node
  nbCELL value;                // assigned value
  struct BTREE_NODE *root ;    // root node for next column
  } BTreeNode;
*/

typedef struct{
  int    options;              // option flags
  struct NB_SET_NODE *root;    // root pointer for the set
  } Set;

#define BTREE_OPTION_TRACE       1  // use closed world assumption

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
    if(strcmp(ident,"trace")==0) options|=BTREE_OPTION_TRACE;
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
  NB_SetNode *node=NULL,**nodeP=&set->root;
  nbCELL argCell;
  nbSET  argSet;

  if(arglist==NULL) return(0); // perhaps we should set the value of the tree itself
  argSet=nbListOpen(context,arglist);
  if(value==NB_CELL_UNKNOWN || value==NB_CELL_FALSE){
    if(argSet==NULL && set->root!=NULL) set->root=nbSetEmpty(set->root);
    else{
      } removeNode(context,tree,&tree->root,&argSet);
    return(0);
    }
  if(argSet==NULL) return(0);
  while((argCell=nbListGetCellValue(context,&argSet))!=NULL){
    nodeP=&tree
    node=nbTreeLocate(&path,argCell,(NB_TreeNode **)nodeP);
    if(node==NULL){
      node=nbAlloc(sizeof(NB_SetNode));
      memset(node,0,sizeof(NB_SetNode));
      nbSetInsert(tree->root,&path,node,argCell);
      //node->bnode.key=(void *)argCell;
      //node->value=NB_CELL_TRUE;
      }
    else nbCellDrop(context,argCell);
//    argCell=nbListGetCellValue(context,&argSet);
    }
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
  NB_SetNode *node=NULL,*root=set->root;

  if(skillHandle->trace || set->options&BTREE_OPTION_TRACE){
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
    node=nbTreeFind(argCell,(NB_TreeNode *)root);
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
*  Internal function to show a node in the tree (row in the table)
*
*    This is used by the show() method
*/
static void setShowNode(nbCELL context,int depth,int column,BTreeNode *node){
  int i;

  if(node->bnode.left!=NULL) setShowNode(context,depth+1,column,(BTreeNode *)node->bnode.left);
  for(i=column;i>=0;i--) nbLogPut(context,"  ");
  nbCellShow(context,(nbCELL)node->bnode.key);
  //nbLogPut(context,"[%d]",depth);
  if(node->value!=NULL){
    nbLogPut(context,"=");
    nbCellShow(context,node->value);
    }  
  nbLogPut(context,"\n");
  if(node->root!=NULL) setShowNode(context,0,column+1,node->root);
  if(node->bnode.right!=NULL) setShowNode(context,depth+1,column,(BTreeNode *)node->bnode.right);
  }

/*
*  show() method
*
*    show <node>;
*
*    show table;
*/
static int setShow(nbCELL context,void *skillHandle,Set *tree,int option){
  if(option!=NB_SHOW_REPORT) return(0);
  //nbLogPut(context,"\n");
  if(tree->root!=NULL) setShowNode(context,0,0,tree->root);
  return(0);
  }

static void treeFlatten(nbCELL context,SetSkill *skillHandle,Set *tree){
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeFlatten called");
  //if(tree->root!=NULL) treeFlattenNode(context,&tree->root,tree->root);  
  if(tree->root!=NULL) nbTreeFlatten((NB_TreeNode **)&tree->root,(NB_TreeNode *)tree->root);  
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeFlatten returning");
  }

static void treeBalance(nbCELL context,SetSkill *skillHandle,Set *tree){
  NB_SetNode *node;
  int n=0;

  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeBalance called");
  if(tree->root!=NULL){
    treeFlatten(context,skillHandle,tree);               // make the tree a list
    for(node=tree->root;node!=NULL;node=(BTreeNode *)node->bnode.right) n++; // count the nodes
    if(n>2) tree->root=(BTreeNode *)nbTreeBalance((NB_TreeNode *)tree->root,n,(NB_TreeNode **)&node);    // balance the tree
    }
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeBalance returning");
  }

/*
*  Convert cell value to cell expression
*    NOTE: This should be a part of the NodeBrain API
*/
static int treeStoreValue(nbCELL context,nbCELL cell,char *cursor,int len){
  int cellType,n=-1;
  double real;
  char *string,number[256];

  if(cell==NB_CELL_UNKNOWN){
    n=1;
    if(n>len) return(-1);
    strcpy(cursor,"?");
    }
  else{
    cellType=nbCellGetType(context,cell);
    if(cellType==NB_TYPE_STRING){
      string=nbCellGetString(context,cell);
      n=strlen(string)+2;
      if(n>len) return(-1);
      sprintf(cursor,"\"%s\"",string);
      }
    else if(cellType==NB_TYPE_REAL){
      real=nbCellGetReal(context,cell);
      sprintf(number,"%.10g",real);
      n=strlen(number);
      if(n>len) return(-1);
      strcpy(cursor,number);
      }
    }
  return(n);
  }

static int treeStoreNode(nbCELL context,SetSkill *skillHandle,BTreeNode *node,FILE *file,char *buffer,char *cursor,char *bufend){
  char *append,*curCol=cursor;
  int n;

  n=treeStoreValue(context,node->bnode.key,cursor,bufend-cursor);
  if(n<0){
    nbLogMsg(context,0,'L',"Row is too large for buffer or cell type unrecognized: %s\n",buffer);
    return(-1);
    }
  cursor+=n;
  if(node->value!=NULL){
    append=cursor;
    if(node->value==NB_CELL_TRUE){
      strcpy(append,");");  // should make sure we have room for this
      }
    else{
      strcpy(append,")=");  // should make sure we have room for this
      append+=2;
      n=treeStoreValue(context,node->value,append,bufend-append);
      if(n<0){
        nbLogMsg(context,0,'L',"Row is too large for buffer or cell type unrecognized: %s\n",buffer);
        return(-1);
        }
      append+=n;
      strcpy(append,";");  // should make sure we have room for this
      }
    fprintf(file,"%s\n",buffer);
    }
  if(node->root !=NULL){
    strcpy(cursor,",");  // should make sure we have room for this
    cursor++;
    treeStoreNode(context,skillHandle,node->root,file,buffer,cursor,bufend);
    }
  if(node->bnode.left!=NULL) treeStoreNode(context,skillHandle,(BTreeNode *)node->bnode.left,file,buffer,curCol,bufend);
  if(node->bnode.right!=NULL) treeStoreNode(context,skillHandle,(BTreeNode *)node->bnode.right,file,buffer,curCol,bufend);
  return(0);
  }

static void setStore(nbCELL context,SetSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
  char buffer[NB_BUFSIZE],*bufend=buffer+sizeof(buffer),*cursor=text,filename[512];
  FILE *file;
  int len;
  BTreeNode *node=NULL;
  nbSET argSet;
  nbCELL argCell;
  void *ptr;


  while(*cursor!=0 && strchr(" ;",*cursor)==NULL) cursor++;
  len=cursor-text;
  if(len>sizeof(filename)-1){
    nbLogMsg(context,0,'E',"File name too large for buffer.");
    return;
    }
  strncpy(filename,text,len);
  *(filename+len)=0;
  if((file=fopen(filename,"w"))==NULL){
    nbLogMsg(context,0,'E',"Unable to open %s",filename);
    return;
    }
  strcpy(buffer,"assert (");
  cursor=buffer+strlen(buffer);

  argSet=nbListOpen(context,arglist);
  if(argSet==NULL){
    if(tree->root!=NULL) treeStoreNode(context,skillHandle,tree->root,file,buffer,cursor,buffer+sizeof(buffer));
    }
  else{
    ptr=&tree->root;
    if(argSet!=NULL){
      while((argCell=nbListGetCellValue(context,&argSet))!=NULL && (node=nbSetFind(argCell,tree))!=NULL){ 
        len=treeStoreValue(context,node->bnode.key,cursor,bufend-cursor);
        if(len<0){
          nbLogMsg(context,0,'L',"Row is too large for buffer or cell type unrecognized: %s\n",buffer);
          fclose(file);  // 2012-12-18 eat - CID 751618
          return;
          }
        cursor+=len;
        if(argSet!=NULL){
          strcpy(cursor,",");
          cursor++;
          } 
        nbCellDrop(context,argCell);
        ptr=&node->root;   
        }
      if(argCell!=NULL){
        nbLogMsg(context,0,'E',"Entry not found.");
        fclose(file);  // 2012-12-18 eat - CID 751618
        return;
        }
      if(node->root!=NULL) treeStoreNode(context,skillHandle,node->root,file,buffer,cursor,buffer+sizeof(buffer));
      }
    }
  fclose(file);
  }

// Prune a tree at the selected node without removing the selected node
//
static void treePrune(nbCELL context,SetSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
  BTreeNode *node=NULL;
  nbSET argSet;
  nbCELL argCell;
  void *ptr;

  argSet=nbListOpen(context,arglist);
  if(argSet==NULL){
    if(tree->root!=NULL){
      removeTree(context,tree,tree->root);
      tree->root=NULL;
      }
    }
  else{
    ptr=&tree->root;
    if(argSet!=NULL){
      while((argCell=nbListGetCellValue(context,&argSet))!=NULL && (node=nbSetFind(argCell,tree))!=NULL){ 
        nbCellDrop(context,argCell);
        ptr=&node->root;   
        }
      if(argCell!=NULL){
        nbLogMsg(context,0,'E',"Entry not found.");
        return;
        }
      if(node->root!=NULL){
        removeTree(context,tree,node->root);
        node->root=NULL;
        }
      }
    }
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

  if(skillHandle->trace || set->options&BTREE_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_tree:setCommand() text=[%s]\n",text);
    }
  len=setGetIdent(&cursor,ident,sizeof(ident));
  if(len<0){
    nbLogMsg(context,0,'E',"Verb not recognized at \"%s\".",cursor);
    return(0);
    }
  while(*cursor==' ') cursor++;
  if(strcmp(ident,"trace")==0){
    len=setGetIdent(&cursor,ident,sizeof(ident));
    if(len==0 || strcmp(ident,"on")) tree->options|=BTREE_OPTION_TRACE;
    else if(strcmp(ident,"off")) tree->options&=0xffffffff-BTREE_OPTION_TRACE;
    }
  else if(strcmp(ident,"flatten")==0) treeFlatten(context,skillHandle,set);
  else if(strcmp(ident,"balance")==0) treeBalance(context,skillHandle,set);
  else if(strcmp(ident,"store")==0) setStore(context,skillHandle,set,arglist,cursor);
  else if(strcmp(ident,"prune")==0) treePrune(context,skillHandle,set,arglist,cursor);
  else nbLogMsg(context,0,'E',"Verb \"%s\" not recognized.",ident);
  return(0);
  }

/*
*  _prune evaluate() method
*
*  2013-12-07 eat - experimenting with facets
*             define fred node tree;
*             fred. assert ("abc","def","xyz");
*             fred. assert ("abc","def","abc");
*             define r1 on(b && x_prune("abc",def");
*             assert b;
*/
static nbCELL setPruneEvaluate(nbCELL context,SetSkill *skillHandle,Set *tree,nbCELL arglist){
  treePrune(context,skillHandle,tree,arglist,"");
  return(tree->notfound);
  }

/*
*  _prune assert() method
*
*  2013-12-07 eat - experimenting with facets
*             define fred node tree;
*             fred. assert ("abc","def","xyz");
*             fred. assert ("abc","def","abc");
*             assert fred_prune("abc","def");
*/
static int setPruneAssert(nbCELL context,void *skillHandle,Set *tree,nbCELL arglist,nbCELL value){
  treePrune(context,skillHandle,tree,arglist,"");
  return(0);
  }

/*
*  _prune command() method
*
*  2013-12-07 eat - experimenting with facets
*             define fred node tree;
*             fred. assert ("abc","def","xyz");
*             fred. assert ("abc","def","abc");
*             fred_prune("abc","def");
*/
static int *setPruneCommand(nbCELL context,SetSkill *skillHandle,Set *tree,nbCELL arglist,char *text){
  treePrune(context,skillHandle,tree,arglist,text);
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
  nbCELL facet; // 2013-12-07 eat - experimenting with facets

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

  // 2013-12-07 eat - experimenting with facets
  facet=nbSkillFacet(context,skill,"prune");
  nbSkillMethod(context,facet,NB_NODE_ASSERT,setPruneAssert);
  nbSkillMethod(context,facet,NB_NODE_EVALUATE,setPruneEvaluate);
  nbSkillMethod(context,facet,NB_NODE_COMMAND,setPruneCommand);
  return(skillHandle);
  }
