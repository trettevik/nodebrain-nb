/*
* Copyright (C) 2003-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     nb_tree.c
*
* Title:    Tree Skill Module
*
* Purpose:
*
*   This file is a NodeBrain skill module for storing and looking
*   up values in a binary tree structure. 
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
*     define table1 node tree;
*     define table2 node tree:partition;
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
*   This new "tree" skill module is functionally the same as the old
*   "tree" skill module with one exception.  Because it is implemented
*   as a binary tree instead of a list, we are able to use a tree to
*   classify values based on a partitioning of the value space.  When
*   used in this way, we return the assigned value for any key equal
*   to or greater than the key in a tree node and less than the next
*   higher key.
*
*   If a complete tree is defined as follows,
*
*       assert fred("abc")="x";
*       assert fred("def")="y";
*
*   then values are returned as follows.
*
*       fred("ab") returns ?
*       fred("abc") returns "x"
*       fred("abz") returns "x"
*       fred("dez") returns "y"
*
*   Otherwise a tree behaves just like before.
*
*      define t1 node tree;
*      assert t1(1,2,3)=4,t1(2,3,4)=5;
*      assert a=2,b=3,c=2;
*
*      define r1 on(t1(a,b,c)=5) x=1;
*
*      assert c=4; # this would cause r1 to fire
* 
*   When a tree leaf is asserted, the branch is defined by the present
*   computed value of each argument cell.
*
*      assert t1(a*b,b+c)=3;  # same as next assertion (see a,b,c values above)
*      assert t1(6,5)=3;   
*
*   Specifying a "found" value different from the "notfound" value enables
*   matching on partial row keys.
*
*      define t2 node tree:found=1;
*
*      assert a,b=2,t2(1,2,3,4);
*      
*      define r2 on(t2(a,b)); # found because matches head of t2(1,2,3,4)
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003/12/19 Ed Trettevik - first prototype
* 2006/08/21 eat 0.6.6  Converted lists to binary trees for better performance
* 2006/08/21 eat 0.6.6  Introduced partitioned tree option - max value<=key matches
* 2006/09/02 eat 0.6.6  Converted to balanced search tree (AVL tree)
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2012-12-18 eat 0.8.13 Checker updates
* 2013-12-27 eat 0.8.13 Removed commented out function treeFind
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

typedef struct BTREE_NODE{
  NB_TreeNode bnode;           // binary tree node
  nbCELL value;                // assigned value
  struct BTREE_NODE *root ;    // root node for next column
  } BTreeNode;

typedef struct BTREE{
  int    options;              // option flags
  nbCELL notfound;             // default value for missing index (defaults to Unknown)
  nbCELL found;                // default value for partial rows (defaults to notfound)
  struct BTREE_NODE *root;
  } BTree;

#define BTREE_OPTION_TRACE       1  // use closed world assumption
#define BTREE_OPTION_ORDER       2  // Order keys by value (otherwise by address)
#define BTREE_OPTION_PARTITION   4  // Match on highest value <= argument

typedef struct BTREE_SKILL{
  char trace;                    /* trace option */
  } BTreeSkill;


// NOTE: This compare function should be part of the NodeBrain API
/*
*  Compare two cells
*
*  Return:
*     -3 c1<c2 because c1 is not recognized and c2 is string
*     -2 c1<c2 because c1 is number and c2 isn't
*     -1 c1<c2 
*      0 c1=c2 string=string, number=number, or both have unrecognized types
*      1 c1>c2
*      2 c1>c2 because c1 is string and c2 isn't
*      3 c1>c2 because c1 is not recognized and c2 is number
*/
static int treeCompare(nbCELL context,nbCELL c1,nbCELL c2){
  int c1Type,c2Type;
  char *c1Str,*c2Str;
  double c1Real,c2Real;

  c1Type=nbCellGetType(context,c1);
  c2Type=nbCellGetType(context,c2);
  if(c1Type==NB_TYPE_STRING){
    if(c2Type==NB_TYPE_STRING){
      c1Str=nbCellGetString(context,c1);
      c2Str=nbCellGetString(context,c2);
      return(strcmp(c1Str,c2Str));
      }
    else return(2);
    }
  else if(c1Type==NB_TYPE_REAL){
    if(c2Type==NB_TYPE_REAL){
      c1Real=nbCellGetReal(context,c1);
      c2Real=nbCellGetReal(context,c2);
      if(c1Real<c2Real) return(-1);
      else if(c1Real==c2Real) return(0);
      else return(1);
      }
    else return(-2);
    }
  else{
    if(c2Type==NB_TYPE_STRING) return(3);
    else if(c2Type==NB_TYPE_REAL) return(-3);
    else return(0);
    }
  }


// Find the next argument
//
// argSetP - address of pointer (argSet) returned by call to nbListOpen(context,&argSet);
//           This is a handle to the remaining arguments
//
// ptr     - This is a pointer to a node pointer. This value my be useful when the argument
//           is not found in addition to when it is found, because it is positioned to where
//           the node should be inserted if not found.
//
// Returns node pointer - NULL if not found.
//
static BTreeNode *treeFindArg(nbCELL context,BTree *tree,nbCELL argCell,void **ptrP){
  BTreeNode **nodeP=*ptrP,*node=*nodeP;
  int rc;

  rc=1;
  while(node!=NULL && rc!=0){
    if(tree->options&BTREE_OPTION_ORDER)
      rc=treeCompare(context,node->bnode.key,argCell);
    else if((nbCELL)node->bnode.key<argCell) rc=-1;
    else if((nbCELL)node->bnode.key>argCell) rc=1;
    else rc=0;
    if(rc<0) *ptrP=(BTreeNode *)&node->bnode.right;
    else if(rc>0) *ptrP=(BTreeNode *)&node->bnode.left;
    nodeP=*ptrP;
    node=*nodeP;
    }
  return(node);  
  }

//===========================================================================================================
/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*/
static void *treeConstruct(nbCELL context,BTreeSkill *skillHandle,nbCELL arglist,char *text){
  int options=0;
  nbCELL found=NULL;
  nbCELL notfound=NULL;
  nbCELL cell;
  BTree *tree;
  char *cursor=text,*delim,ident[256];
  int len;

  if(skillHandle!=NULL && skillHandle->trace) nbLogMsg(context,0,'T',"sample.treeConstruct() called");
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
    else if(strcmp(ident,"order")==0) options|=BTREE_OPTION_ORDER;
    else if(strcmp(ident,"partition")==0) options|=BTREE_OPTION_PARTITION|BTREE_OPTION_ORDER;
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
  tree=(BTree *)nbAlloc(sizeof(BTree));
  tree->options=options;
  tree->notfound=notfound;
  tree->found=found;
  tree->root=NULL;
  return(tree);
  }

//
// Recursively remove all nodes in a binary tree
// 
static void *removeTree(nbCELL context,BTree *tree,BTreeNode *node){
  node->bnode.key=nbCellDrop(context,node->bnode.key);
  if(node->value!=NULL) node->value=nbCellDrop(context,node->value);
  if(node->bnode.left!=NULL) node->bnode.left=removeTree(context,tree,(BTreeNode *)node->bnode.left);
  if(node->bnode.right!=NULL) node->bnode.right=removeTree(context,tree,(BTreeNode *)node->bnode.right);
  if(node->root!=NULL) node->root=removeTree(context,tree,node->root);
  nbFree(node,sizeof(BTreeNode));
  return(NULL);
  }

/* 
*  Internal function to remove a node from a tree - used by assert() method
*
*  Returns:
*    0 - don't remove the parent node
*    1 - consider removing the parent node (if no value or root)
*    2 - remove the parent node (even if it has a value and/or root)
*/
static int removeNode(nbCELL context,BTree *tree,BTreeNode **nodeP,nbSET *argSetP){
  NB_TreePath path;
  BTreeNode *node=*nodeP;
  nbCELL argCell;
  int code=1;

  argCell=nbListGetCellValue(context,argSetP);
  if(argCell==NULL){
    if(node==NULL) return(3);  // the perfect match - nothing to nothing
    code=2; 
    return(2);                   //
    }
  else{
    if(node==NULL) return(0);  // can't match to empty tree
    if(tree->options&BTREE_OPTION_ORDER) 
      node=(BTreeNode *)nbTreeLocateValue(&path,argCell,(NB_TreeNode **)nodeP,treeCompare,context);
    else node=nbTreeLocate(&path,argCell,(NB_TreeNode **)nodeP);   
    nbCellDrop(context,argCell);
    if(node==NULL) return(0);          // didn't find argument
    switch(removeNode(context,tree,&node->root,argSetP)){
      case 0: return(0);
      case 1:
        if(node->value!=NULL || node->root!=NULL) return(0);  // still need this node
        break;
      // For case 2 we just fall thru to unlink the node
      }
    }
  if(node->value!=NULL) node->value=nbCellDrop(context,node->value); // release value
  if(node->root!=NULL) return(0);  
  nbTreeRemove(&path);  // Remove node from binary search tree
  if(node->bnode.key!=NULL) node->bnode.key=nbCellDrop(context,(nbCELL)node->bnode.key);  // release key
  //if(node->root!=NULL) node->root =removeTree(context,tree,node->root);
  nbFree(node,sizeof(BTreeNode));
  return(code);
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
static int treeAssert(nbCELL context,void *skillHandle,BTree *tree,nbCELL arglist,nbCELL value){
  NB_TreePath path;
  BTreeNode *node=NULL,**nodeP=&tree->root;
  nbCELL argCell;
  nbSET  argSet;

  if(arglist==NULL) return(0); // perhaps we should set the value of the tree itself
  argSet=nbListOpen(context,arglist);
  if(value==NB_CELL_UNKNOWN){
    if(argSet==NULL && tree->root!=NULL) tree->root=removeTree(context,tree,tree->root);
    else removeNode(context,tree,&tree->root,&argSet);
    return(0);
    }
  if(argSet==NULL) return(0);
  argCell=nbListGetCellValue(context,&argSet);
  while(argCell!=NULL){
    if(tree->options&BTREE_OPTION_ORDER) 
      node=nbTreeLocateValue(&path,argCell,(NB_TreeNode **)nodeP,treeCompare,context);
    else node=nbTreeLocate(&path,argCell,(NB_TreeNode **)nodeP);
    if(node==NULL){
      node=nbAlloc(sizeof(BTreeNode));
      memset(node,0,sizeof(BTreeNode));
      nbTreeInsert(&path,(NB_TreeNode *)node);
      nodeP=&(node->root);
      while((argCell=nbListGetCellValue(context,&argSet))!=NULL){
        node=nbAlloc(sizeof(BTreeNode));
        memset(node,0,sizeof(BTreeNode));
        node->bnode.key=(void *)argCell;
        *nodeP=node;
        nodeP=&(node->root);
        }
      node->value=(nbCELL)nbCellGrab(context,value);
      return(0);
      }
    nodeP=&(node->root);
    nbCellDrop(context,argCell);
    argCell=nbListGetCellValue(context,&argSet);
    }
  /* matched - change value */
  if(node->value!=NULL) nbCellDrop(context,node->value);
  node->value=nbCellGrab(context,value);
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
static nbCELL treeEvaluate(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist){
  nbCELL    argCell;
  nbSET     argSet;
  BTreeNode *node=NULL,*root=tree->root;

  if(skillHandle->trace || tree->options&BTREE_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_tree::treeEvaluate()");
    nbLogPut(context,"tree");
    if(arglist!=NULL) nbCellShow(context,arglist);
    nbLogPut(context,"\n");
    }
  if(arglist==NULL){  // request for tree value
    return(NB_CELL_UNKNOWN);  // for now, let it be Unknown
    }
  argSet=nbListOpen(context,arglist);
  if(argSet==NULL) return(tree->notfound); // tree() returns default value
  argCell=nbListGetCellValue(context,&argSet);
  while(argCell!=NULL && argSet!=NULL){
    if(tree->options&BTREE_OPTION_ORDER){
      if(tree->options&BTREE_OPTION_PARTITION)
        node=nbTreeFindFloor(argCell,(NB_TreeNode *)root,treeCompare,context);
      else node=nbTreeFindValue(argCell,(NB_TreeNode *)root,treeCompare,context); 
      }
    else node=nbTreeFind(argCell,(NB_TreeNode *)root);
    if(node==NULL){
      nbCellDrop(context,argCell);
      return(tree->notfound);
      }
    nbCellDrop(context,argCell);
    argCell=nbListGetCellValue(context,&argSet);
    root=node->root;
    }
  // matched on arguments
  if(node->value==NULL) return(tree->found);
  return(node->value);
  }

/*
*  Internal function to show a node in the tree (row in the table)
*
*    This is used by the show() method
*/
static void treeShowNode(nbCELL context,int depth,int column,BTreeNode *node){
  int i;

  if(node->bnode.left!=NULL) treeShowNode(context,depth+1,column,(BTreeNode *)node->bnode.left);
  for(i=column;i>=0;i--) nbLogPut(context,"  ");
  nbCellShow(context,(nbCELL)node->bnode.key);
  //nbLogPut(context,"[%d]",depth);
  if(node->value!=NULL){
    nbLogPut(context,"=");
    nbCellShow(context,node->value);
    }  
  nbLogPut(context,"\n");
  if(node->root!=NULL) treeShowNode(context,0,column+1,node->root);
  if(node->bnode.right!=NULL) treeShowNode(context,depth+1,column,(BTreeNode *)node->bnode.right);
  }

/*
*  show() method
*
*    show <node>;
*
*    show table;
*/
static int treeShow(nbCELL context,void *skillHandle,BTree *tree,int option){
  if(option!=NB_SHOW_REPORT) return(0);
  //nbLogPut(context,"\n");
  if(tree->root!=NULL) treeShowNode(context,0,0,tree->root);
  return(0);
  }

static void treeFlatten(nbCELL context,BTreeSkill *skillHandle,BTree *tree){
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeFlatten called");
  //if(tree->root!=NULL) treeFlattenNode(context,&tree->root,tree->root);  
  if(tree->root!=NULL) nbTreeFlatten((NB_TreeNode **)&tree->root,(NB_TreeNode *)tree->root);  
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"treeFlatten returning");
  }

static void treeBalance(nbCELL context,BTreeSkill *skillHandle,BTree *tree){
  BTreeNode *node;
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

static int treeStoreNode(nbCELL context,BTreeSkill *skillHandle,BTreeNode *node,FILE *file,char *buffer,char *cursor,char *bufend){
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

static void treeStore(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
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
      while((argCell=nbListGetCellValue(context,&argSet))!=NULL && (node=treeFindArg(context,tree,argCell,&ptr))!=NULL){ 
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
static void treePrune(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
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
      while((argCell=nbListGetCellValue(context,&argSet))!=NULL && (node=treeFindArg(context,tree,argCell,&ptr))!=NULL){ 
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

static int treeGetIdent(char **cursorP,char *ident,int size){
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
static int *treeCommand(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
  char *cursor=text,ident[512];
  int len;

  if(skillHandle->trace || tree->options&BTREE_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_tree:treeCommand() text=[%s]\n",text);
    }
  len=treeGetIdent(&cursor,ident,sizeof(ident));
  if(len<0){
    nbLogMsg(context,0,'E',"Verb not recognized at \"%s\".",cursor);
    return(0);
    }
  while(*cursor==' ') cursor++;
  if(strcmp(ident,"trace")==0){
    len=treeGetIdent(&cursor,ident,sizeof(ident));
    if(len==0 || strcmp(ident,"on")) tree->options|=BTREE_OPTION_TRACE;
    else if(strcmp(ident,"off")) tree->options&=0xffffffff-BTREE_OPTION_TRACE;
    }
  else if(strcmp(ident,"flatten")==0) treeFlatten(context,skillHandle,tree);
  else if(strcmp(ident,"balance")==0) treeBalance(context,skillHandle,tree);
  else if(strcmp(ident,"store")==0) treeStore(context,skillHandle,tree,arglist,cursor);
  else if(strcmp(ident,"prune")==0) treePrune(context,skillHandle,tree,arglist,cursor);
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
static nbCELL treePruneEvaluate(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist){
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
static int treePruneAssert(nbCELL context,void *skillHandle,BTree *tree,nbCELL arglist,nbCELL value){
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
static int *treePruneCommand(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
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
extern void *treeBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  BTreeSkill *skillHandle;
  char *cursor=text;
  nbCELL facet; // 2013-12-07 eat - experimenting with facets

  skillHandle=(BTreeSkill *)nbAlloc(sizeof(BTreeSkill));
  skillHandle->trace=0;
  while(*cursor==' ') cursor++;
  while(*cursor!=0 && *cursor!=';'){
    if(strncmp(cursor,"trace",5)==0){
      skillHandle->trace=1;
      cursor+=5;
      }
    else{
      nbLogMsg(context,0,'T',"Option not recognized at \"%s\".",cursor);
      nbFree(skillHandle,sizeof(BTreeSkill));
      return(NULL);
      }
    while(*cursor==' ' || *cursor==',') cursor++;
    }
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,treeConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,treeAssert);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,treeEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,treeShow);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,treeCommand);

  // 2013-12-07 eat - experimenting with facets
  facet=nbSkillFacet(context,skill,"prune");
  nbSkillMethod(context,facet,NB_NODE_ASSERT,treePruneAssert);
  nbSkillMethod(context,facet,NB_NODE_EVALUATE,treePruneEvaluate);
  nbSkillMethod(context,facet,NB_NODE_COMMAND,treePruneCommand);
  return(skillHandle);
  }
