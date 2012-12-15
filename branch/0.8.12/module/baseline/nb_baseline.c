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
* File:     nb_baseline.c
*
* Title:    Statistical Anomaly Detection Skill Module
*
* Purpose:
*
*   This file is a NodeBrain skill module for detecting statistical anomalies
*   in measures based on an exponentially weighted moving average (ewma) and
*   an exponentially weighted moving deviation.
* 
* Reference:
*
*   See Baseline NodeBrain Module Manual for more information.
*
* Syntax:
*
*   Node Definition:
*
*     define <node> node <skill>[(args)][:text]
*
*     define baseline1 node baseline("<directory>",<weight>,<tolerance>,<cycle>,<interval>)[:<options>];
*
*     <directory>  - directory where baseline files are stored
*     <weight>     - number (real number) specifying weight (sometimes called lamda) of new values relative to old ewma 
*     <tolerance>  - number (real number) of deviations to tolerate
*     <cycle>      - number of minutes in cycle (time before wrapping around on baseline files) 
*     <interval>   - number of minutes covered by each baseline file (must divide evenly into cycle) 
*     <options>    - comma separated options
*                    "sum"    - add asserted values over each period and only check upper limits until end of period
*                    "static" - profile is not modified (use in cases where training is complete)
*
*   Command:
*
*     <node>[(args)][:text]
*
*     .(name1,name2,...):set average,deviation;
*
*   Cell Expression:
*
*     ... <node>(args) ...
*
*     returns true if measure is known
*
*   Assertion:
*
*     <node>. assert (args)=<number>,...;
*     assert <node>(args)=<number>,...;
*     define r1 on(condition) <node>(args)=<number>,...;
*
*
* Description:
*   
*   The Baseline node module monitors a set of measures and alerts when
*   a measure is outside a normal range.  The normal range for a given
*   measure is defined by two exponentially weighted moving averages:
*
*     1) the average measure, and 
*     2) the average deviation from the average measure.
*
*   Deviation thresholds are defined at powers of 2 times the average
*   deviation times a "tolerance" factor.  The tolerance is expressed
*   by the user in units of sigma (standard deviation), which we
*   approximate as 1.25 times the average deviation.  The tolerance is 
*   converted to units of average deviation for internal use.  The first
*   threshold (level 0) is tolerance times average deviation.  The second
*   (level 1) is twice the first.  The third is twice the second, etc.
*
*     threshold = 2**level * tolerance * average_deviation
*
*   We define deviation as the absolute value of the difference between
*   a measure's current value and average value.  By checking this against
*   the current threshold, we are actually checking an upper and lower
*   limit.
*
*     upper_limit = average_value + threshold
*     lower_limit = average_value - threshold
*
*     (deviation > threshold)<==>(value < lower_limit or value > upper_limit)
*
*   The level indicates the "magnitude" of an anomaly.  We use an exponential
*   scale (powers of 2) to avoid generating a large number of alerts.  Each
*   time an alert is generated the threshold is doubled (the level increments
*   by one).  At the end of a period, the level is decremented as much as
*   possible without dropping below the current value.  At the start of a
*   period, when the average_value and average_deviation are reset, the
*   threshold is recomputed using the current level.  This prevents generation
*   of alerts at the start of each period during an anomalous episode of a
*   given magnitude.
*   
*   At the end of a complete period, the average and deviation for each measure
*   is computed and stored in a baseline file for the period.  The file
*   name is nnnnnnnn.nb where nnnnnnnn is a multiple of the period and less than
*   the cycle time.
*
*   The format of a baseline file is a set of node commands.
*
*    .("name1","name2",...):set <average>,<deviation>;
*
*   This enables a "source" command to be used to load the baseline for each
*   period.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2010-08-02 Ed Trettevik - first prototype
* 2010-09-24 eat - Changed "limit" variable to "level" 
* 2010-09-24 eat - Changed level to mean powers of 2
* 2010-09-26 eat - included threshold in node to avoid recomputing on every assertion
*=============================================================================
*/
#include "config.h"
#include <nb/nb.h>
#include <nb/nbtree.h>

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

/* Anomaly skill and node structures */

typedef struct BTREE_NODE{
  NB_TreeNode bnode;           // binary tree node
  // Baseline variables
  double average;              // exponentially weighted moving average
  double deviation;            // exponentially weighted moving deviation
  // Temporary variables
  double value;                // current value of the measure
  double threshold;            // current threshold
  int    level;                // power of 2 for current limits to alert on
  //
  struct BTREE_NODE *root;     // root node for next column
  } BTreeNode;

typedef struct BTREE{
  int    options;              // option flags
  nbCELL notfound;             // default value for missing index (defaults to Unknown)
  nbCELL found;                // default value for partial rows (defaults to notfound)
  char  *directory;            // directory where baseline files are stored
  double weight;               // weighting factor
  double tolerance;            // tolerance defining normal range of values
  int    cycle;                // cycle time in seconds
  int    periods;              // number of periods within a cycle
  int    period;               // period number within a week
  int    interval;             // seconds in a period
  nbCELL synapse;              // timer cell
  struct BTREE_NODE *root;
  } BTree;

#define BTREE_OPTION_TRACE       1  // use closed world assumption
#define BTREE_OPTION_ORDER       2  // Order keys by value (otherwise by address)
#define BTREE_OPTION_PARTITION   4  // Match on highest value <= argument
#define BTREE_OPTION_SUM         8  // Sum values over each period
#define BTREE_OPTION_STATIC     16  // Don't update baseline profile

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


// 
// Find a node pointer for a given argument list
//   For now we return null if not found
//   In the future we need modify it so we can use it with assertions as well
//
// Updated Argument:
//   *nodeP is address of pointer to node when found or pointer to where it could
//   be inserted if not found.
//
// Returns:
//   0 not found 
//   1 found
//
// Note: To use this for inserts we'll need to pass &argSet instead of arglist
//       so we know which arguments need to be inserted.
//
//static BTreeNode *treeFind(nbCELL context,BTree *tree,nbCELL arglist,void **ptr){
//  BTreeNode *node=NULL,**rootP=&tree->root;
//  nbSET argSet;
//  nbCELL argCell;
//  int rc;
//
//  *ptr=*rootP;
//  argSet=nbListOpen(context,arglist);
//  argCell=nbListGetCellValue(context,&argSet);
//  while(argCell!=NULL){
//    node=*rootP;
//    rc=1;
//    while(node!=NULL && rc!=0){
//      if(tree->options&BTREE_OPTION_ORDER)
//        rc=treeCompare(context,node->bnode.key,argCell);
//      else if((nbCELL)node->bnode.key<argCell) rc=-1;
//      else if((nbCELL)node->bnode.key>argCell) rc=1;
//      else rc=0;
//      if(rc<0) rootP=(BTreeNode **)&node->bnode.right;
//      else if(rc>0) rootP=(BTreeNode **)&node->bnode.left;
//      node=*rootP;
//      }
//    *ptr=*rootP;
//    if(node==NULL) return(node);
//    nbCellDrop(context,argCell);
//    argCell=nbListGetCellValue(context,&argSet);
//    rootP=&node->root;
//    }
//  return(node);
//  }

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
*
*    define <term> node baseline("<directory>",<weight>,<tolerance>,<cycle>,<interval>)[:<options>];
*/
static void *baselineConstruct(nbCELL context,BTreeSkill *skillHandle,nbCELL arglist,char *text){
  int options=0;
  nbCELL found=NULL;
  nbCELL notfound=NULL;
  nbCELL cell;
  BTree *tree;
  char *cursor=text,*delim,ident[256];
  int len;
  nbSET  argSet;
  char *directory;
  double weight=0.5,tolerance=3,cycle=7*24*60,periods=24;
  double interval=60;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Baseline directory string required as first argument");
    return(NULL);
    }
  directory=nbCellGetString(context,cell);  // we don't drop this cell because we reference it
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    if(nbCellGetType(context,cell)!=NB_TYPE_REAL || (weight=nbCellGetReal(context,cell))<0 || weight>1){
      nbLogMsg(context,0,'E',"Second argument must be numeric weight between 0 and 1");
      return(NULL);
      }
    nbCellDrop(context,cell);
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      if(nbCellGetType(context,cell)!=NB_TYPE_REAL || (tolerance=nbCellGetReal(context,cell))<0){
        nbLogMsg(context,0,'E',"Third argument must be non-negative numeric tolerance");
        return(NULL);
        }
      nbCellDrop(context,cell);
      cell=nbListGetCellValue(context,&argSet);
      if(cell!=NULL){
        if(nbCellGetType(context,cell)!=NB_TYPE_REAL || (cycle=nbCellGetReal(context,cell))<1 || cycle!=(int)cycle){
          nbLogMsg(context,0,'E',"Forth argument must be positive integer number of minutes in a cycle");
          return(NULL);
          }
        nbCellDrop(context,cell);
        cell=nbListGetCellValue(context,&argSet);
        if(cell!=NULL){
          if(nbCellGetType(context,cell)!=NB_TYPE_REAL || (interval=nbCellGetReal(context,cell))<1 || interval!=(int)interval || (periods=cycle/interval)<1 || periods!=(int)periods){
            nbLogMsg(context,0,'E',"Fifth argument must be positive integer number of minutes in an interval by which %d is evenly divisable",cycle);
            return(NULL);
            }
          nbCellDrop(context,cell);
          cell=nbListGetCellValue(context,&argSet);
          if(cell!=NULL){
            nbLogMsg(context,0,'E',"Only five arguments supported");
            return(NULL);
            }
          }
        }
      }
    }
  if(skillHandle!=NULL && skillHandle->trace) nbLogMsg(context,0,'T',"baselineConstruct() called");
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
    else if(strcmp(ident,"sum")==0) options|=BTREE_OPTION_SUM;
    else if(strcmp(ident,"static")==0) options|=BTREE_OPTION_STATIC;
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
  tree->directory=directory;
  tree->cycle=cycle*60;
  tree->periods=periods;
  tree->interval=interval*60;
  tree->weight=weight;
  tree->tolerance=tolerance*1.25;
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(tree);
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

static void baselineAlert(nbCELL context,BTreeSkill *skillHandle,BTree *tree,BTreeNode *node,nbCELL element[],int qualifiers,double deviation){
  char cmd[1024];
  char *cmdcur;
  int i;
  double limit,threshold;

  //nbLogMsg(context,0,'T',"baselineAlert called with qualifiers=%d threshold=%g",qualifiers,deviation);
  strcpy(cmd,"alert _measure=\"");
  cmdcur=strchr(cmd,0);
  for(i=0;i<qualifiers;i++){
    switch(nbCellGetType(context,element[i])){
      case NB_TYPE_STRING:
        sprintf(cmdcur,"%s.",nbCellGetString(context,element[i]));
        break;
      case NB_TYPE_REAL:
        sprintf(cmdcur,"%f.",nbCellGetReal(context,element[i]));
        break;
      default:
        strcpy(cmdcur,"?.");
      }
    cmdcur=strchr(cmdcur,0);
    }
  if(*(cmdcur-1)=='.') cmdcur--;
  if(node->value<node->average) limit=node->average-node->threshold;
  else limit=node->average+node->threshold;
  node->level++;
  if(node->threshold<=0) return; // make sure we don't loop here
  while((threshold=node->threshold*2) && deviation>threshold) node->level++,node->threshold=threshold;
  sprintf(cmdcur,"\",_value=%.10g,_average=%.10g,_sigma=%.10g,_deviation=%.10g,_threshold=%.10g,_limit=%.10g,_level=%d;",node->value,node->average,node->deviation*1.25,deviation,node->threshold,limit,node->level);
  nbActionCmd(context,cmd,0);
  node->threshold=threshold;
  }

static int treeStoreNode(nbCELL context,BTreeSkill *skillHandle,BTree *tree,int learning,BTreeNode *node,nbCELL element[],int qualifiers,FILE *file,char *buffer,char *cursor,char *bufend){
  char *curCol=cursor;
  int n;
  double threshold,deviation;

  element[qualifiers]=(nbCELL)node->bnode.key;
  qualifiers++;
  //nbLogMsg(context,0,'T',"treeStoreNode: called tree=%p node=%p root=%p",tree,node,node->root);
  n=treeStoreValue(context,node->bnode.key,cursor,bufend-cursor);
  if(n<0){
    nbLogMsg(context,0,'L',"Row is too large for buffer or cell type unrecognized: %s\n",buffer);
    return(-1);
    }
  cursor+=n;
  if(learning){
    if(node->average==0 && node->deviation==0){
      node->average=node->value;
      node->deviation=node->value/4;  // start with 25% deviation
      node->threshold=node->deviation*tree->tolerance;
      node->level=0;
      }
    else{
      // check deviation against threshold 
      deviation=fabs(node->value-node->average);
      if(deviation>node->threshold) baselineAlert(context,skillHandle,tree,node,element,qualifiers,deviation);
      else while(node->level && (threshold=node->threshold/2) && deviation<threshold) node->level--,node->threshold=threshold;
      node->deviation+=tree->weight*(deviation-node->deviation);
      node->average+=tree->weight*(node->value-node->average);
      } 
    if(file) fprintf(file,"%s):set %.10g,%.10g;\n",buffer,node->average,node->deviation);
    if(tree->options&BTREE_OPTION_SUM) node->value=0;  // reset value when summing
    }
  else if(file) fprintf(file,"%s)=%.10g;\n",buffer,node->value);
  if(node->root!=NULL){
    strcpy(cursor,",");  // should make sure we have room for this
    cursor++;
    treeStoreNode(context,skillHandle,tree,learning,node->root,element,qualifiers,file,buffer,cursor,bufend);
    }
  qualifiers--;
  if(node->bnode.left!=NULL) treeStoreNode(context,skillHandle,tree,learning,(BTreeNode *)node->bnode.left,element,qualifiers,file,buffer,curCol,bufend);
  if(node->bnode.right!=NULL) treeStoreNode(context,skillHandle,tree,learning,(BTreeNode *)node->bnode.right,element,qualifiers,file,buffer,curCol,bufend);
  //nbLogMsg(context,0,'T',"treeStoreNode: returning tree=%p node=%p root=%p",tree,node,node->root);
  return(0);
  }

static void treeStore(nbCELL context,BTreeSkill *skillHandle,BTree *tree,char *text){
  char buffer[NB_BUFSIZE],*cursor=text,filename[512];
  FILE *file=NULL;
  int qualifiers=0;
  nbCELL element[32];
  int learning=1;
  int len;

  if(text){
    learning=0;
    while(*cursor!=0 && strchr(" ;",*cursor)==NULL) cursor++;
    len=cursor-text;
    if(len>sizeof(filename)-1){
      nbLogMsg(context,0,'E',"File name too large for buffer.");
      return;
      }
    strncpy(filename,text,len);
    *(filename+len)=0;
    }
  else sprintf(filename,"%s/%8.8d.nb",tree->directory,tree->period*tree->interval);
  // write to file
  if(!(tree->options&BTREE_OPTION_STATIC) && (file=fopen(filename,"w"))==NULL){
    nbLogMsg(context,0,'E',"Unable to open %s",filename);
    return;
    }
  if(tree->root){
    if(learning) strcpy(buffer,".(");
    else strcpy(buffer,"assert (");
    cursor=buffer+strlen(buffer);
    treeStoreNode(context,skillHandle,tree,learning,tree->root,element,qualifiers,file,buffer,cursor,buffer+sizeof(buffer));
    }
  if(file) fclose(file);
  }

static void treeLoad(nbCELL context,void *skillHandle,BTree *tree){
  time_t utime;
  int cycleTime;
  int period;
  char filename[512];

  time(&utime);
  cycleTime=utime%tree->cycle;
  period=cycleTime/tree->interval;
  tree->period=period;
  sprintf(filename,"%s/%8.8d.nb",tree->directory,tree->period*tree->interval);
  nbSource(context,filename); // load using the command interpreter
  }

static void treeAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  BTree *tree=(BTree *)nodeHandle;
  int remaining;
  time_t utime;

  if(tree->period>=0){ // store baseline
    treeStore(context,skillHandle,tree,NULL);
    }
  treeLoad(context,skillHandle,tree); // load baseline
  // reset timer
  time(&utime);
  remaining=tree->interval-utime%tree->interval;
  //nbLogMsg(context,0,'T',"treeAlarm() called for baseline %s - interval=%d,remaining=%d",tree->directory,tree->interval,remaining);
  nbSynapseSetTimer(context,tree->synapse,remaining);
  }

/*
*  enable() method
*
*    enable <node>
*
*/
int baselineEnable(nbCELL context,void *skillHandle,BTree *tree){
  int remaining;
  time_t utime;
  if(tree->options&BTREE_OPTION_TRACE) nbLogMsg(context,0,'T',"baselineEnable() called for baseline %s",tree->directory);
  tree->synapse=nbSynapseOpen(context,skillHandle,tree,NULL,treeAlarm);
  treeLoad(context,skillHandle,tree); // load baseline
  // set timer
  time(&utime);
  remaining=tree->interval-utime%tree->interval;
  nbSynapseSetTimer(context,tree->synapse,remaining);
  nbLogMsg(context,0,'I',"Enabled baseline %s",tree->directory);
  nbLogFlush(context);
  return(0);
  }

//
// Recursively remove all nodes in a binary tree
// 
static void *removeTree(nbCELL context,BTree *tree,BTreeNode *node){
  node->bnode.key=nbCellDrop(context,node->bnode.key);
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
        if(node->root!=NULL) return(0);  // still need this node
        break;
      // For case 2 we just fall thru to unlink the node
      }
    }
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
static int baselineAssert(nbCELL context,void *skillHandle,BTree *tree,nbCELL arglist,nbCELL value){
  NB_TreePath path;
  BTreeNode *node=NULL,**nodeP=&tree->root;
  nbCELL argCell;
  nbSET  argSet;
  int cellType;
  double real;
  int qualifiers=0;
  nbCELL element[32];
  double deviation;

  if(arglist==NULL) return(0); // perhaps we should set the value of the tree itself
  argSet=nbListOpen(context,arglist);
  if(value==NB_CELL_UNKNOWN){
    if(argSet==NULL && tree->root!=NULL) tree->root=removeTree(context,tree,tree->root);
    else removeNode(context,tree,&tree->root,&argSet);
    return(0);
    }
  cellType=nbCellGetType(context,value);
  if(cellType!=NB_TYPE_REAL){
    nbLogMsg(context,0,'E',"Value must be a number");
    return(0);
    }
  real=nbCellGetReal(context,value);
  if(argSet==NULL) return(0);
  argCell=nbListGetCellValue(context,&argSet);
  while(argCell!=NULL){
    element[qualifiers]=argCell;
    qualifiers++;
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
      node->value=real;
      // note we don't set deviation, threshold and level until the end of the period.
      return(0);
      }
    nodeP=&(node->root);
    nbCellDrop(context,argCell);
    argCell=nbListGetCellValue(context,&argSet);
    }
  if(tree->options&BTREE_OPTION_SUM){
    // need to check for zero average and deviation here!
    node->value+=real;
    if(node->average==0 && node->deviation==0) return(0);
    deviation=fabs(node->value-node->average);
    if(node->value>node->average && deviation>node->threshold) baselineAlert(context,skillHandle,tree,node,element,qualifiers,deviation);
    }
  else{
    node->value=real;  // update value and check limits
    if(node->average==0 && node->deviation==0) return(0);
    deviation=fabs(node->value-node->average);
    if(deviation>node->threshold) baselineAlert(context,skillHandle,tree,node,element,qualifiers,deviation);
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
static nbCELL baselineEvaluate(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist){
  nbCELL    argCell;
  nbSET     argSet;
  BTreeNode *node=NULL,*root=tree->root;

  if(skillHandle->trace || tree->options&BTREE_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_tree::baselineEvaluate()");
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
  // return(tree->found);    // could alternatively return a real object with value of node->value
  return(nbCellCreateReal(context,node->value));
  }

/*
*  Internal function to show a node in the tree (row in the table)
*
*    This is used by the show() method
*/
static void baselineShowNode(nbCELL context,int depth,int column,BTreeNode *node){
  int i;

  if(node->bnode.left!=NULL) baselineShowNode(context,depth+1,column,(BTreeNode *)node->bnode.left);
  for(i=column;i>=0;i--) nbLogPut(context,"  ");
  nbCellShow(context,(nbCELL)node->bnode.key);
  nbLogPut(context,"=%.10g,a=%.10g,d=%.10g,l=%d\n",node->value,node->average,node->deviation*1.25,node->level);
  if(node->root!=NULL) baselineShowNode(context,0,column+1,node->root);
  if(node->bnode.right!=NULL) baselineShowNode(context,depth+1,column,(BTreeNode *)node->bnode.right);
  }

/*
*  show() method
*
*    show <node>;
*
*    show table;
*/
static int baselineShow(nbCELL context,void *skillHandle,BTree *tree,int option){
  if(option!=NB_SHOW_REPORT){
    nbLogPut(context," weight=%f tolerance=%f",tree->weight,tree->tolerance/1.25);
    return(0);
    }
  //nbLogPut(context,"\n");
  if(tree->root!=NULL) baselineShowNode(context,0,0,tree->root);
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

static void treeSet(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
  NB_TreePath path;
  BTreeNode *node=NULL,**nodeP=&tree->root;
  nbSET argSet;
  nbCELL argCell;
  double average,deviation;

  average=strtod(text,&text);
  while(*text==' ') text++;
  if(*text!=','){
    nbLogMsg(context,0,'E',"Expecting ',' at: %s",text);
    return;
    }
  text++;
  deviation=strtod(text,&text);
  while(*text==' ') text++;
  if(*text!=';'){
    nbLogMsg(context,0,'E',"Expecting ';' at: %s",text);
    return;
    }
  if(arglist==NULL || (argSet=nbListOpen(context,arglist))==NULL){
    nbLogMsg(context,0,'E',"Expecting argument list");
    return;
    }
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
      node->average=average;
      node->deviation=deviation;
      node->threshold=deviation*tree->tolerance;
      node->level=0;
      return;
      }
    nodeP=&(node->root);
    nbCellDrop(context,argCell);
    argCell=nbListGetCellValue(context,&argSet);
    }
  if(node){
    /* matched - change value */
    node->average=average;
    node->deviation=deviation;
    node->threshold=(double)((int)1<<node->level)*deviation*tree->tolerance;
    }
  return;
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
static int *baselineCommand(nbCELL context,BTreeSkill *skillHandle,BTree *tree,nbCELL arglist,char *text){
  char *cursor=text,ident[512];
  int len;

  if(skillHandle->trace || tree->options&BTREE_OPTION_TRACE){
    nbLogMsg(context,0,'T',"nb_baseline:baselineCommand() text=[%s]\n",text);
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
    else nbLogMsg(context,0,'E',"Trace argument \"%s\" not recognized.",ident);
    }
  else if(strcmp(ident,"flatten")==0) treeFlatten(context,skillHandle,tree);
  else if(strcmp(ident,"balance")==0) treeBalance(context,skillHandle,tree);
  else if(strcmp(ident,"set")==0) treeSet(context,skillHandle,tree,arglist,cursor);
  else if(strcmp(ident,"store")==0) treeStore(context,skillHandle,tree,cursor);
  else if(strcmp(ident,"prune")==0) treePrune(context,skillHandle,tree,arglist,cursor);
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
extern void *baselineBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  BTreeSkill *skillHandle;
  char *cursor=text;

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
  /* Still trying to figure out if we want to require method binding */
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,baselineConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,baselineEnable);
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,baselineAssert);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,baselineEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,baselineShow);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,baselineCommand);
  return(skillHandle);
  }
