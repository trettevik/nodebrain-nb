/*
* Copyright (C) 1998-2012 The Boeing Company
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
* File:     nbnode.c 
*
* Title:    Node Management Routines
*
* Function:
*
*   This file provides routines that manage Nodebrain nodes.  A node
*   combines skill and knowledge.  The knowledge is represented as a glossary
*   of terms and definitions.  A skill module may be used to provide a special
*   skill and associated knowledge representation.
*  
* Synopsis:
*
*   #include "nb.h"
*
*   void nbNodeInit();
*
*   NB_Node *nbNodeParse();
*
* Description
*
*   [2005/04/09 eat - this section needs review - may not be correct any longer.]
*
*   You can construct a node using the nbNodeParse() method.
*
*     NB_Node *mycon=nbNodeParse();
*
*   The caller is responsible for incrementing the use count of a node when
*   references are assigned.  The nbNodeParse() routine increments the reference
*   count of identity and source as appropriate using grabObject().  Your
*   call to grabObject for the context returned would look like this.
*
*     mypointer=grabObject(mycon);
*
*   A call to dropObject is required when the pointer is changed.
*
*     dropObject(NB_Node *mycontext);
*
*   The node object is a publisher without a value of interest, and is not
*   currently a subscriber.  It is "subscribed to" by node conditions.  It
*   only publishes when the associated node is updated, and the call to 
*   nbCellPublish() is currently in the parser.
*
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/07/05 Ed Trettevik (original prototype version 0.2.8)
* 2001/07/09 eat - version 0.2.8
*             1) modified to use grabObject() and dropObject();
* 2002/08/20 eat - version 0.4.0
*             1) Included an alertContext() function to proxy for the
*                alertCache() function.  In the future we should make CACHE
*                a proper OBJECT (or NB_CELL) so we can alarm it directly.
* 2002/08/25 eat - version 0.4.0
*             1) split nbcontext.c out of nbcontext.h
* 2003/11/23 eat 0.5.5  Merged nbskill.c and nbcontext.c into nbnode.c
* 2005/04/27 eat 0.6.2  Included alert method for cache module.
*            This method is not something that will be advertised because I'm
*            not sure where it is going.  I just need a way to enable the 
*            cache module to continue providing the functionality that the
*            old entangled cache code was able to support. 
* 2005/05/14 eat 0.6.3  module handle passed to skill bind methods
* 2007/06/26 eat 0.6.8  Changing terminology ("expert" is now "node")
* 2007/07/16 eat 0.6.8  renamed from nbexpert.c to nbnode.c
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-10-17 eat 0.8.12 Replaced termGetName with nbTermName
*=============================================================================
*/
#include <nb/nbi.h>
#if !defined(WIN32)
#include <sys/times.h>
#endif

/*
struct TYPE *contextType;
*/
NB_Node *nb_NodeFree=NULL; /* free nodes */

/*
*  Context object constructor
*/
NB_Node *nbNodeNew(void){
  NB_Node *node;
  node=nbCellNew(nb_NodeType,(void **)&nb_NodeFree,sizeof(NB_Node));
  node->context=NULL;          /* handle for this context - name and glossary */
  node->reference=NULL;       /* reference term   this=.=that  */
  node->owner=clientIdentity; /* owner's identity */
  node->source=(struct STRING *)nb_Unknown;  /* command to request unknown values */
  node->cell.object.value=nb_Disabled;
  node->ifrule=NULL;
  node->cmdopt=0;             /* default to no echo for now */
  node->skill=NULL;
  node->facet=NULL;
  node->knowledge=NULL;       /* e.g. cache table */
  node->alertCount=0;
  return(node);
  }

void contextAlert(NB_Term *term){
  struct ACTION *action;
  NB_Node *node=(NB_Node *)term->def;
  if(trace) outMsg(0,'T',"contextAlert() called.");
  if(node->cell.object.type!=nb_NodeType){
    outMsg(0,'L',"contextAlert() called with term not defined as node.");
    printObject((NB_Object *)term);
    outPut("\n");
    return;
    }
  node->alertCount++;
  for(action=((NB_Node *)term->def)->ifrule;action!=NULL;action=(struct ACTION *)action->cell.object.next){
    // 2013-11-03 eat - experimenting with rules having the full range of values found in any cell - multiple true
    //if(action->cond!=NULL && ((NB_Object *)action->cond)->value==NB_OBJECT_TRUE){ /* if true put on action list */
    // 2013-12-05 eat - switched to using an object type attribute to exclude untrue values - can the value ever be NULL?
    if(action->cond!=NULL && !(((NB_Object *)action->cond)->value->type->attributes&TYPE_NOT_TRUE)){ /* if true put on action list */
    //if(action->cond!=NULL &&
    //  ((NB_Object *)action->cond)->value!=NB_OBJECT_FALSE &&
    //  ((NB_Object *)action->cond)->value!=nb_Unknown &&
    //  ((NB_Object *)action->cond)->value!=nb_Disabled){ /* if true put on action list */
      if(action->status=='R') scheduleAction(action);  /* put ready rule on scheduled list */
      }
    }
  if(actList!=NULL || nb_RuleReady!=NULL) nbRuleReact();
  }

void nbNodeAlert(nbCELL context,nbCELL node){
  contextAlert((NB_Term *)node);
  }

/*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbskill.c 
*
* Title:    Skill Function Interface Support 
*
* Function:
*
*   This file provides methods that map the NodeBrain environment to skill
*   modules.  It supports NB_SKILL objects and NB_SKILLCALL functions for
*   definition, evaluation, assertion and command processing.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbSkillInit();
*   struct NB_SKILL *nbSkillParse(NB_Term *context,char *cursor);
*
* 
* Description
*
*   The functions defined here conform to NodeBrain internals, and map
*   the internal concepts of "solve", "eval", "assert", and "command"
*   to skill handler functions provided by skill modules.  Internal
*   concepts like subscribe and publish are, for now, handled here and
*   not exposed to the skill handler functions.  The goal is to keep the
*   skill handler interface relatively simple.
*
*   NodeBrain C API documentation describes the interface for programmers
*   of skill modules.  Here's a split second overview.
*
*     o  A skill handler function has a parameter list that includes a
*        handle to a NodeBrain context, request code, text, foreign handle,
*        and an argument list.
*
*     o  This request code enables a single skill handler function to perform
*        multiple functions on behalf of NodeBrain.  These include translating
*        foreign object descriptions, evaluating conditions, asserting
*        conditions, and interpreting command foreign to NodeBrain.
*
*     o  The text parameter is used to pass text to a skill handler for
*        parsing and interpretation.
*
*     o  The foreign handle may be used by the skill handler to associate a
*        data structure with a skill function when it is defined.  This 
*        structure may then be referenced when handling various operations
*        for the skill function.
*
*     o  The argument list provides a variable number of NodeBrain objects
*        as arguments to a skill handler.
*        
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003/04/23 eat 0.5.4  Introduced prototype
* 2009/02/14 eat 0.7.5  Modified to clean up when node spec has syntax error
*=============================================================================
*/

NB_Type *skillType=NULL;
NB_Type *facetType=NULL;
NB_Type *condTypeNode=NULL;
NB_Type *nb_NodeType=NULL;

//struct HASH *nb_SkillHash=NULL;    /* skill term hash */
NB_Term *nb_SkillGloss=NULL;

/*
* We don't hash skill objects because we don't know enough to share them.
* It is up to skill modules to manage their objects well.
*/
/* struct NB_SKILL *usedSkills=NULL; */  /* used skill list */
struct NB_SKILL *freeSkills=NULL;  /* free skill list */


/**********************************************************************
* Private Object Methods
**********************************************************************/
void printSkill(struct NB_SKILL *skill){
  outPut("%s ",skill->object.type->name);
  outFlush(); // debug
  printStringRaw(skill->ident);
  outFlush(); // debug
  if(skill->args!=NULL) printObject((NB_Object *)skill->args);
  if(skill->text!=NULL && *(skill->text->value)!=0){
    outPut(":");
    printStringRaw(skill->text);
    }
  }

void nbNodeCellShow(struct NB_CALL *call){
  if(call==NULL) outPut("(?)");
  else{
    printObject((NB_Object *)call->term);
    printObject((NB_Object *)call->args);
    }
  }

void destroySkill(struct NB_SKILL *skill){
  }

void nbNodeShowItem(struct NB_NODE *node){
  outPut("node ");
  if(node->skill==NULL) return;
  //outPut("%s",node->skill->term->word->value);
  outPut("%s",node->skill->ident->value);
  (*node->facet->show)(node->context,node->skill->handle,node->knowledge,NB_SHOW_ITEM);
  }

void nbNodeShowReport(struct NB_NODE *node){
  if(node->skill!=NULL) (*node->facet->show)(node->context,node->skill->handle,node->knowledge,NB_SHOW_REPORT);
  if(node->source!=NULL && node->source!=(void *)nb_Unknown){
    outPut("\n  source: ");
    printObject((NB_Object *)node->source);
    outPut("\n");
    }
  }

void nbNodeDestroy(NB_Node *node){
  if(node->skill!=NULL && node->knowledge!=NULL)
    (*node->facet->destroy)(node->context,node->skill->handle,node->knowledge);
  dropObject((NB_Object *)node->source);
  node->cell.object.next=(NB_Object *)nb_NodeFree;
  nb_NodeFree=node;
  }

/* using destroyCondition() for NB_CALL */

/**********************************************************************
* Private Cell Calculation Methods
**********************************************************************/
NB_Object *evalNode(struct NB_NODE *node){
  if(node->facet==NULL) return(nb_Unknown);
  return((*node->facet->eval)(node->context,node->skill->handle,node->knowledge,NULL));
  }

void solveNode(struct NB_NODE *node){
  if(node->facet==NULL) return;
  (*node->facet->solve)(node->context,node->skill->handle,node->knowledge,NULL);
  return;
  }

NB_Object *evalNodeCall(struct NB_CALL *call){
  NB_Node *node=(NB_Node *)call->term->def;
  if(node->cell.object.type!=nb_NodeType) return(nb_Unknown);
  if(node->facet==NULL) return(nb_Unknown);
  return((*node->facet->eval)(node->context,node->skill->handle,node->knowledge,call->args));
  }

void solveNodeCall(struct NB_CALL *call){
  nbCellSolve_((NB_Cell *)call->args);
  return;
  }

/**********************************************************************
* Private Cell Management Methods
**********************************************************************/
void alarmNode(struct NB_NODE *node){
  if(node->facet==NULL) return;
  (*node->facet->alarm)(node->context,node->skill->handle,node->knowledge);
  }
void enableNode(struct NB_NODE *node){
  if(node->facet==NULL) return;
  (*node->facet->enable)(node->context,node->skill->handle,node->knowledge);
  }
void disableNode(struct NB_NODE *node){
  if(node->facet==NULL) return;
  (*node->facet->disable)(node->context,node->skill->handle,node->knowledge);
  }

void enableNodeCall(struct NB_CALL *call){
  nbCellEnable((NB_Cell *)call->term,(NB_Cell *)call);
  nbCellEnable((NB_Cell *)call->args,(NB_Cell *)call);
  }
void disableNodeCall(struct NB_CALL *call){
  nbCellDisable((NB_Cell *)call->term,(NB_Cell *)call);
  nbCellDisable((NB_Cell *)call->args,(NB_Cell *)call);
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbNodeInit(NB_Stem *stem){
  condTypeNode=newType(stem,"nodeCell",condH,0,nbNodeCellShow,destroyCondition);
  nbCellType(condTypeNode,solveNodeCall,evalNodeCall,enableNodeCall,disableNodeCall);
  nb_NodeType=newType(stem,"node",NULL,TYPE_ENABLES,nbNodeShowItem,nbNodeDestroy);
  nb_NodeType->apicelltype=NB_TYPE_NODE;
  nbCellType(nb_NodeType,solveNode,evalNode,enableNode,disableNode);
  nb_NodeType->showReport=&nbNodeShowReport;
  nb_NodeType->alarm=&alarmNode;

  nb_SkillGloss=nbTermNew(NULL,"skill",nbNodeNew());
  skillType=newType(stem,"skill",NULL,0,printSkill,destroySkill);
  facetType=newType(stem,"facet",NULL,0,NULL,NULL);
  }

void *nbSkillNullConstruct(struct NB_TERM *context,void *skillHandle,NB_Cell *arglist,char *text){
  return(nb_Unknown); // Returning unknown to avoid NULL because NULL means construction failed
  }
void *nbSkillNullDestroy(struct NB_TERM *context,void *skillHandle,void *objectHandle){
  return(NULL);
  }
void nbSkillNullShow(struct NB_TERM *context,void *skillHandle,void *objectHandle,int option){
  return;
  }
int nbSkillNullEnable(struct NB_TERM *context,void *skillHandle,void *objectHandle){
  return(0);
  }
int nbSkillNullDisable(struct NB_TERM *context,void *skillHandle,void *objectHandle){
  return(0);
  }
int nbSkillNullAssert(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value){
  return(0);
  }
struct NB_OBJECT *nbSkillNullEval(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args){
  return(nb_Unknown);
  }
struct NB_OBJECT *nbSkillNullCompute(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args){
  return(nb_Unknown);
  }
void nbSkillNullSolve(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args){
  return;
  }
int nbSkillNullCommand(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args,char *text){
  return(0);
  }
void nbSkillNullAlarm(struct NB_TERM *context,void *skillHandle,void *objectHandle){
  outMsg(0,'L',"Node alarmed without alarm skill.");
  }

#if !defined(WIN32)
int nbSkillTraceAlert(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value){
  int rc;
  clock_t ticks;
  struct tms tms_times;
  struct NB_FACET_SHIM *shim=((NB_Node *)context->def)->skill->facet->shim;

  times(&tms_times);
  ticks=tms_times.tms_utime;
  outMsg(0,'T',"Tracing %s skill alert method - call",context->word->value);
  rc=(*shim->alert)(context,skillHandle,objectHandle,arglist,value);
  times(&tms_times);
  ticks=tms_times.tms_utime-ticks;
  shim->alertTicks+=ticks;
  outMsg(0,'T',"Tracing %s skill alert method - return  cumulative=%le ticks=%le  utime=%le stime=%le",context->word->value,(double)shim->alertTicks,(double)ticks,(double)tms_times.tms_utime,(double)tms_times.tms_stime);
  return(rc);
  }

int nbSkillTraceAssert(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value){
  int rc;
  clock_t ticks;
  struct tms tms_times;
  struct NB_FACET_SHIM *shim=((NB_Node *)context->def)->skill->facet->shim;

  times(&tms_times);
  ticks=tms_times.tms_utime;
  outMsg(0,'T',"Tracing %s skill assert method - call",context->word->value);
  rc=(*shim->assert)(context,skillHandle,objectHandle,arglist,value);
  times(&tms_times);
  ticks=tms_times.tms_utime-ticks;
  shim->assertTicks+=ticks;
  outMsg(0,'T',"Tracing %s skill assert method - return  cumulative=%le ticks=%le  utime=%le stime=%le",context->word->value,(double)shim->assertTicks,(double)ticks,(double)tms_times.tms_utime,(double)tms_times.tms_stime);
  return(rc);
  }
#endif

/*
*  Skill constructor - we are using useCondition() for condTypeNode
*/
/*
* Constructor - not intended as a public method
*/
struct NB_SKILL *nbSkillNew(char *ident,NB_List *args,char *text){
  NB_Skill *skill;

  skill=(NB_Skill *)newObject(skillType,NULL,sizeof(NB_Skill));
  skill->term=NULL;
  skill->status=0;
  skill->handle=NULL;
  skill->ident=grabObject(useString(ident));
  skill->args=args;
  skill->text=grabObject(useString(text));
  skill->facet=nbFacetNew(skill,"");
  return(skill);
  }

struct NB_FACET *nbFacetNew(NB_Skill *skill,const char *ident){
  NB_Facet *facet;

  facet=(NB_Facet *)newObject(facetType,NULL,sizeof(NB_Facet));
  facet->skill=skill;
  facet->ident=grabObject(useString((char *)ident));
  facet->construct=&nbSkillNullConstruct;
  facet->destroy=&nbSkillNullDestroy;
  facet->show=&nbSkillNullShow;
  facet->enable=&nbSkillNullEnable;
  facet->disable=&nbSkillNullDisable;
  facet->assert=&nbSkillNullAssert;
  facet->eval=&nbSkillNullEval;
  facet->compute=&nbSkillNullCompute;
  facet->solve=&nbSkillNullSolve;
  facet->command=&nbSkillNullCommand;
  facet->alarm=&nbSkillNullAlarm;
  facet->alert=&nbSkillNullAssert;
  return(facet);
  }

NB_List *nbSkillArgs(NB_Term *context,char **source){
  NB_List *args=NULL;
  char *cursor=*source;
  /* parse the argument list - we need to include foreign expression support here */
  if(*cursor!='('){
    outMsg(0,'E',"Expecting argument list at \"%s\"",cursor);
    return(NULL);
    }
  cursor++;
  args=parseList(context,&cursor); /* include parameter for foreign expression parsing */
  if(*cursor!=')'){
    outMsg(0,'E',"Expecting right parenthesis at \"%s\"",cursor);
    return(NULL);
    }
  cursor++;
  while(*cursor==' ') cursor++;
  *source=cursor;
  return(args);
  }

struct NB_SKILL *nbSkillParse(NB_Term *context,char *cursor){
  struct NB_SKILL *skill;
  NB_List *args=NULL;
  char *cursave,symid,ident[256],*text;
  cursave=cursor;
  symid=nbParseSymbol(ident,sizeof(ident),&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting skill identifier [<module>.]<symbol>() at \"%s\"",cursave);
    return(NULL);
    }
  if(*cursor=='(') args=grabObject(nbSkillArgs(context,&cursor));
  if(*cursor==':'){
    text=cursor+1;
    while(*text==' ') text++;
    }
  else{
    if(*cursor!=0 && *cursor!=';'){
      outMsg(0,'E',"Expecting colon ':' or end of command at:%s",cursor);
      if(args!=NULL) dropObject(args);
      return(NULL);
      }
    text="";
    }
  skill=nbSkillNew(ident,args,text);
  return(skill);
  }

NB_Term *nbNodeParse(NB_Term *context,char *ident,char *cursor){
  NB_Node *node;
  NB_Skill *skill;
  NB_Facet *facet;
  NB_Term *term,*skillTerm;
  NB_List *args=NULL;
  char symid,token[256],*cursave,*tokenP=token,msg[256];
  NB_SKILL_BIND skillBind;
  void *moduleHandle=NULL;

  node=nbNodeNew();
  term=nbTermNew(context,ident,node);
  node->context=term;

  while(*cursor==' ') cursor++;
  cursave=cursor;
  if(*cursor=='>'){
    outMsg(0,'T',"Command redirection recognized");
    cursor++;
    cursave=cursor;
    symid=nbParseSymbol(token,sizeof(token),&cursor);
    if(symid!='t'){
      outMsg(0,'E',"Expecting node term at: %s",cursave);
      termUndef(term);
      return(NULL);
      }
    }
  symid=nbParseSymbol(token,sizeof(token),&cursor);
  if(symid==';') return(NULL);
  if(symid!='t'){
    outMsg(0,'E',"Expecting skill name or end of line at: %s",cursave);
    termUndef(term);
    return(NULL);
    }
  if(NULL==(skillTerm=nbTermFind(nb_SkillGloss,token)) || skillTerm->def==nb_Undefined){
    /* implicitly declare the skill if necessary */
    if((node->skill=nbSkillParse(context,tokenP))==NULL){
      outMsg(0,'E',"Skill \"%s\" not declared",token);
      termUndef(term);
      return(NULL);
      }
    else node->skill->term=nbTermNew(nb_SkillGloss,token,node->skill);
    }
  else node->skill=(NB_Skill *)skillTerm->def;
  if(*cursor=='(') args=grabObject(nbSkillArgs(context,&cursor));
  if(*cursor==':') cursor++;
  else if(*cursor!=0 && *cursor!=';'){
    outMsg(0,'E',"Expecting colon ':' or end of command at:%s",cursor);
    dropObject(args);
    termUndef(term);
    return(NULL);
    }
  skill=node->skill;
  if(skill->status==0){  // bind skills on first reference
    if((skillBind=(NB_SKILL_BIND)nbModuleSymbol(context,skill->ident->value,"Bind",&moduleHandle,msg,sizeof(msg)))==NULL){
      //outPut("%s\n",msg);
      outMsg(0,'E',"%s",msg);
      dropObject(args);
      termUndef(term);
      return(NULL);
      }
    skill->handle=(*skillBind)(term,moduleHandle,skill,skill->args,skill->text->value);
    skill->status=1;
    }
  node->facet=skill->facet;  // use primary facet for now
  facet=skill->facet;  // use primary facet for now
  node->knowledge=(*facet->construct)(term,skill->handle,(NB_Cell *)args,cursor);
  if(node->knowledge==NULL){
    dropObject(args);
    termUndef(term);
    return(NULL);
    }
  dropObject(args);
  return(term);
  } 

int nbNodeCmd(nbCELL context,char *name,char *cursor){
  NB_Node *node=NULL;
  NB_Skill *skill=NULL;
  NB_Facet *facet=NULL;
  NB_List *args=NULL;
  NB_Term *term;
  char *cursave,symid,ident[64];

  if(NULL==(term=nbTermFind((NB_Term *)context,name))){
    outMsg(0,'E',"Node \"%s\" not defined.",name);
    return(-1);
    }
  if(term->def->type!=nb_NodeType){
    outMsg(0,'E',"Term \"%s\" not defined as node.",name);
    return(-1);
    }
  node=(NB_Node *)term->def;
  skill=node->skill;
  facet=node->facet;
  //facet=skill->facet;
  if(facet==NULL){
    outMsg(0,'E',"Node \"%s\" does not have a command method.",name);
    return(-1);
    }
  if(*cursor=='_'){
    cursor++;
    cursave=cursor;
    symid=nbParseSymbol(ident,sizeof(ident),&cursor);
    if(symid!='t'){
      outMsg(0,'E',"Expecting facet identifier at->%s",cursave);
      return(-1);
      }
    facet=nbSkillGetFacet(skill,ident);
    if(!facet){
      outMsg(0,'E',"Expecting facet identifier at->%s",cursave);
      return(-1);
      }
    }
  if(*cursor=='(') args=grabObject(nbSkillArgs((NB_Term *)context,&cursor));
  if(*cursor==':') cursor++;
  else if(*cursor==';') cursor="";
  else if(*cursor!=0){
    outMsg(0,'E',"Expecting colon ':' or end of command at:%s",cursor);
    dropObject(args);
    return(-1);
    }
  (*facet->command)(term,skill->handle,node->knowledge,args,cursor);
  if(args!=NULL) dropObject(args);
  return(0);
  }

int nbNodeCmdIn(nbCELL context,nbCELL args,char *text){
  NB_Node *node=NULL;
  NB_Skill *skill=NULL;
  NB_Facet *facet=NULL;
  NB_Term *term=(NB_Term *)context;

  if(term->def->type!=nb_NodeType){
    outMsg(0,'E',"Term \"%s\" not defined as node.",term->word->value);
    return(-1);
    }
  node=(NB_Node *)term->def;
  skill=node->skill;
  facet=node->facet;
  //facet=skill->facet;
  if(facet==NULL){
    outMsg(0,'E',"Node \"%s\" does not have a command method.",term->word->value);
    return(-1);
    }
  (*facet->command)((NB_Term *)context,skill->handle,node->knowledge,(NB_List *)args,text);
  return(0);
  }

//**********************************
// External API

char *nbNodeGetName(nbCELL context){
  return(((NB_Term *)context)->word->value);
  }

char *nbNodeGetNameFull(nbCELL context,char *name,size_t size){
  nbTermName(name,size,(NB_Term *)context,NULL);
  return(name);
  }

void *nbNodeGetKnowledge(nbCELL context,nbCELL cell){
  return(((NB_Node *)cell)->knowledge);
  }

int nbNodeSetLevel(nbCELL context,nbCELL cell){
  NB_Node *node=(NB_Node *)((NB_Term *)context)->def;
  int level;

  level=cell->level+1;
  if(level>node->cell.level) node->cell.level=level;
  nbCellLevel((NB_Cell *)node);
  return(level);
  }

void nbNodeSetValue(nbCELL context,nbCELL cell){
  NB_Node *node=(NB_Node *)((NB_Term *)context)->def;
  NB_Object *object=&node->cell.object;

  if(object->value!=nb_Disabled) dropObject(object->value);
  object->value=grabObject(cell);
  }
