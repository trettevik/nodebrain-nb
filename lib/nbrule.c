/*
* Copyright (C) 1998-2014 The Boeing Company
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
* File:     nbrule.c
*
* Title:    Rule Object Routines
*
* Function:
*
*   This file provides functions supporting NodeBrain rules.  Rules are
*   procedural objects that run concurrently.  Conceptually, each rule runs
*   as a seperate thread.
*
* Synopsis:
*
*   #include "nb.h"
*
*   char *nbRuleParse(nbCELL context,int opt,char **source,char *msg);
*
* Description:
*
*.  A plan is a procedural NodeBrain construct based on lower
*   level declarative constructs. Concurrent rules are enabled via the
*   underlying event scheduling mechanism. 
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-10-31 Ed Trettevik (started original C prototype)
* 2004-08-28 eat 0.6.1  Included drop after compute for IF statement
* 2007-07-22 eat 0.6.8  Corrected rule assertion and command firing sequence error
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-26 dtl - Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
* 2014-01-13 eat 0.9.00 Removed hash pointer - referenced via type
*            Rule objects are not assigned keys currently
*=============================================================================
*/
#include <nb/nbi.h>
#include <stddef.h>

static char *nbRuleParseBody(NB_Cell *context,int opt,NB_Plan *plan,char *ip,int counter,char *source,char **cursorP,char *msg,size_t msglen);

NB_Plan *nb_PlanFree=NULL;
NB_Type *nb_PlanType;

NB_Rule *nb_RuleFree=NULL;   /* Free rule list */
NB_Rule *nb_RuleReady=NULL;  /* Ready rule list - ready to take action */

NB_Type *nb_RuleTypeDeaf;  /* Type used when rule should not respond to alerts */
NB_Type *nb_RuleType;
//NB_Hash *nb_RuleHash;   


/* Step rule to next plan value */
NB_Object *nbRuleStep(NB_Rule *rule){
  NB_PlanInstr *ip=rule->ip;
  while((ip=(*(ip->op))(rule,ip))!=NULL);
  return(rule->cell.object.value);
  }

/*
*  Flow control instructions
*/

NB_PlanInstrP nbPlanLoopBegin(NB_Rule *rule,struct NB_PLAN_LOOP_BEGIN *ip){
  rule->counter[ip->counter]=ip->count;
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_LOOP_BEGIN)));
  }

NB_PlanInstrP nbPlanLoopEnd(NB_Rule *rule,struct NB_PLAN_LOOP_END *ip){
  rule->counter[ip->counter]--;
  if((rule->counter[ip->counter])>0) return((NB_PlanInstrP)((char *)ip+ip->jump));
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_LOOP_END)));
  }

NB_PlanInstrP nbPlanBranch(NB_Rule *rule,struct NB_PLAN_BRANCH *ip){
  return((NB_PlanInstrP)((char *)ip+ip->jump));
  }

NB_PlanInstrP nbPlanIf(NB_Rule *rule,struct NB_PLAN_IF *ip){
  NB_Object *value;
  int doif=0;
  value=(*ip->cond->object.type->compute)(ip->cond);
  if(value==NB_OBJECT_FALSE || value==nb_Unknown) doif=1;
  dropObject(value);
  if(doif) return((NB_PlanInstrP)((char *)ip+ip->jump));
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_IF)));
  }

/*
*  Wait for a condition to be true
*/ 
NB_PlanInstrP nbPlanOnEnable(NB_Rule *rule,struct NB_PLAN_COND *ip){
  nbCellEnable((NB_Cell *)ip->cond,(NB_Cell *)rule); /* subscribe to conditon */
  rule->cond=ip->cond;
  rule->cell.level=ip->cond->level+1;
  rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_COND));
  return(NULL);
  }

NB_PlanInstrP nbPlanWhenEnable(NB_Rule *rule,struct NB_PLAN_COND *ip){
  nbCellEnable(ip->cond,(NB_Cell *)rule); /* subscribe to conditon */
  rule->cond=ip->cond;
  rule->cell.level=ip->cond->level+1;
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_COND)));
  }

NB_PlanInstrP nbPlanWhenTest(NB_Rule *rule,struct NB_PLAN_COND *ip){
  NB_Object *value=(ip->cond)->object.value;
  if(value==NB_OBJECT_FALSE || value==nb_Unknown){
    rule->ip=(NB_PlanInstrP)ip;  /* wait for true condition */
    return(NULL);
    }
  nbCellDisable((NB_Cell *)ip->cond,(NB_Cell *)rule); /* unsubscribe */
  rule->cond=NULL;
  rule->time=nb_ClockTime;  /* continue from current time */
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_COND)));
  }

/*
*  Instructions that set next alert time
*/ 
NB_PlanInstrP nbPlanStep(NB_Rule *rule,struct NB_PLAN_STEP *ip){
  rule->time=(ip->step)(rule->time,ip->count);
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_STEP)));
  }
NB_PlanInstrP nbPlanAlign(NB_Rule *rule,struct NB_PLAN_ALIGN *ip){
  bfi f,s;
  int count=ip->count;
  long begin=rule->time,end;

  end=begin+24*60*60;
  f=tcCast(begin,end,ip->tcdef);
  s=f->next;
  while(f->start<maxtime && count>0){
    if(s!=f){
      if(s->start>begin){
        count--;
        begin=s->start;
        }
      s=s->next;
      }
    else{
      bfiDispose(f);
      end+=end-begin;
      if(end<begin) end=maxtime;
      f=tcCast(begin,end,ip->tcdef);
      s=f->next;
      }
    }
  bfiDispose(f);
  rule->time=begin;
  return((NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_ALIGN)));
  }

NB_PlanInstrP nbPlanWait(NB_Rule *rule,struct NB_PLAN_WAIT *ip){
  if(trace) outMsg(0,'T',"nbPlanWait() called");
  rule->offset=ip->offset;  /* source code offset */
  if(rule->time>nb_ClockTime){
    outMsg(0,'T',"nbPlanWait() setting timer");
    nbClockSetTimer(rule->time,(NB_Cell *)rule);
    rule->state=NB_RuleStateTimer;
    rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_WAIT));
    return(NULL);
    }
  else rule->time=nb_ClockTime; 
  /*
  *  If we have gotten behind schedule, we catch up at a wait point.
  *  This may not be appropriate for all applications, so we should consider
  *  a global option or rule option to control how we handle this situation.
  */
  return(rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_WAIT)));
  }

/* Different version of wait instruction used for time rules */
NB_PlanInstrP nbPlanWaitTime(NB_Rule *rule,struct NB_PLAN_WAIT *ip){
  rule->offset=ip->offset;  /* source code offset */
  rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_WAIT));
  return(NULL);
  }

/*
*  Instructions that set values
*/
NB_PlanInstrP nbPlanDefine(NB_Rule *rule,struct NB_PLAN_VALUE *ip){
  NB_Object *value=ip->value;
  if(rule->valDef!=value){
    nbCellDisable((NB_Cell *)rule->valDef,(NB_Cell *)rule);
    rule->valDef=value;
    nbCellEnable((NB_Cell *)rule->valDef,(NB_Cell *)rule);
    }
  value=value->value;
  if(rule->cell.object.value!=value){
    rule->cell.object.value=value;
    nbCellPublish((NB_Cell *)rule);
    }
  return(rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_VALUE)));
  }

NB_PlanInstrP nbPlanValue(NB_Rule *rule,struct NB_PLAN_VALUE *ip){
  rule->valDef=ip->value->value;
  if(rule->cell.object.value!=rule->valDef){
    rule->cell.object.value=rule->valDef;
    nbCellPublish((NB_Cell *)rule);
    }
  return(rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_VALUE)));
  }
/* Different version of value assignment used in time rules */
NB_PlanInstrP nbPlanValueTime(NB_Rule *rule,struct NB_PLAN_VALUE *ip){
  rule->valDef=ip->value;
  return(rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_VALUE)));
  }

/*
*  Action instructions
*/
NB_PlanInstrP nbPlanAssert(NB_Rule *rule,struct NB_PLAN_ASSERT *ip){
  rule->assertions=ip->assertion;
  rule->nextReady=nb_RuleReady;
  nb_RuleReady=rule;
  rule->state=NB_RuleStateReady;
  rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_ASSERT));
  return(NULL);
  }

NB_PlanInstrP nbPlanCommand(NB_Rule *rule,struct NB_PLAN_COMMAND *ip){
  rule->command=ip->cmd;
  rule->nextReady=nb_RuleReady;
  nb_RuleReady=rule;
  rule->state=NB_RuleStateReady;
  rule->ip=(NB_PlanInstrP)((char *)ip+sizeof(struct NB_PLAN_COMMAND));
  return(NULL);
  }
 
/*
*  Instructions that return
*/

NB_PlanInstrP nbPlanExit(NB_Rule *rule,struct NB_PLAN_VALUE *ip){
  rule->state=NB_RuleStateStopped;  /* all done */
  rule->ip=NULL;
  return(NULL);
  }

/*
*  Parse rule statement
* 
*    opt: 0 - normal rule, 1 - time rule   
*/

char *nbRuleParseStatement(nbCELL context,int opt,NB_Plan *plan,char *ip,int counter,char *source,char **cursorP,char *msg,size_t msglen){
  char *cursor=*cursorP;
  char *ipsave,*cursave,*curstmt=*cursorP;
  struct NB_PLAN_LOOP_BEGIN  *tmLoopBegin;
  struct NB_PLAN_LOOP_END    *tmLoopEnd;
  struct NB_PLAN_BRANCH      *tmBranch;
  struct NB_PLAN_STEP        *tmStep;
  struct NB_PLAN_ALIGN       *tmAlign;
  struct NB_PLAN_COND        *tmCondEnable,*tmCondTest;
  struct NB_PLAN_IF          *tmIf;
  struct NB_PLAN_VALUE       *tmValue;
  struct NB_PLAN_ASSERT      *tmAssert;
  struct NB_PLAN_COMMAND     *tmCommand;
  struct NB_PLAN_WAIT        *tmWait;
  NB_Object *value;
  NB_Link *member;
  int count=0;
  char symid,sign,ident[256];
  long (*step)(long start,int count);
  tc tcdef;

  /* check for / and \ first because nbParseSymbol has deprecated support for time delays */
  if(strchr("/\\",*cursor)!=NULL){
    tmWait=(struct NB_PLAN_WAIT *)ip;
    if(opt==0) tmWait->op=(NB_PlanOp)&nbPlanWait;
    else tmWait->op=(NB_PlanOp)&nbPlanWaitTime;
    tmWait->offset=curstmt-source;
    ip+=sizeof(struct NB_PLAN_WAIT);
    if(*cursor=='/') value=NB_OBJECT_TRUE;
    else value=NB_OBJECT_FALSE;
    cursor++;
    tmValue=(struct NB_PLAN_VALUE *)ip;
    if(opt==0) tmValue->op=(NB_PlanOp)&nbPlanValue;
    else tmValue->op=(NB_PlanOp)&nbPlanValueTime;
    tmValue->value=value;
    ip+=sizeof(struct NB_PLAN_VALUE);
    *cursorP=cursor;
    return(ip);
    }
  cursave=cursor;
  symid=nbParseSymbol(ident,sizeof(ident),&cursor);
  if(symid=='*' || symid=='i' || symid=='-' || symid=='+' || (opt==1 && (symid=='t' || symid=='{' || symid=='('))){
    if(symid=='t' || symid=='{' || symid=='('){
      count=1;
      cursor=cursave;
      }
    else if(symid=='i') count=atoi(ident);
    else if(symid=='-' || symid=='+'){
      sign=symid;
      symid=nbParseSymbol(ident,sizeof(ident),&cursor);
      if(symid!='i'){
        snprintf(msg,msglen,"Expecting integer at -->%s",cursave);
        return(NULL);
        }
      count=atoi(ident);           // 2013-01-01 eat - VID 4954-0.8.13-1
      if(count<0 || count>=1024){
        snprintf(msg,msglen,"Expecting integer n, where 0<n<1024, at -->%s",cursave);
        return(NULL);
        }
      if(sign=='-') count=-count;  // 2013-01-01 eat - VID 4954-0.8.13-1
      }
    if(*cursor=='{'){
      cursor++;   /* step over '{' */
      if(count<0){
        snprintf(msg,msglen,"Negative repeat count on procedure at -->%s",cursor);
        return(NULL);
        }
      if(count>1){
        tmLoopBegin=(struct NB_PLAN_LOOP_BEGIN *)ip;
        tmLoopBegin->op=(NB_PlanOp)&nbPlanLoopBegin;
        tmLoopBegin->counter=counter;
        tmLoopBegin->count=count;
        counter++;
        ip+=sizeof(struct NB_PLAN_LOOP_BEGIN);
        }
      ipsave=ip;
      if(NULL==(ip=nbRuleParseBody(context,opt,plan,ip,counter,source,&cursor,msg,msglen))) return(NULL);
      cursor++;    /* step over '}' - trust nbRuleParseBody to be there */
      if(count>1){
        counter--;
        tmLoopEnd=(struct NB_PLAN_LOOP_END *)ip;
        tmLoopEnd->op=(NB_PlanOp)&nbPlanLoopEnd;
        tmLoopEnd->counter=counter;
        tmLoopEnd->jump=ipsave-ip;   /* negative jump */
        ip+=sizeof(struct NB_PLAN_LOOP_END);
        }
      else if(count==0){
        tmBranch=(struct NB_PLAN_BRANCH *)ip;
        tmBranch->op=(NB_PlanOp)&nbPlanBranch;
        tmBranch->jump=ipsave-ip;    /* negative jump */
        ip+=sizeof(struct NB_PLAN_BRANCH);
        }
      }
    else if(count==0){
      snprintf(msg,msglen,"NB000E Expecting '{' at \"%s\".",cursor);
      return(NULL);
      }
    else if(*cursor=='('){
      if(count<0){
        snprintf(msg,msglen,"NB000E Negative step not currently supported on time condition at \"%s\".",cursor);
        return(NULL);
        }
      if(NULL==(tcdef=tcParse(context,&cursor,msg,msglen))) return(NULL);
      tmAlign=(struct NB_PLAN_ALIGN *)ip;
      tmAlign->op=(NB_PlanOp)&nbPlanAlign;
      tmAlign->count=count;
      tmAlign->tcdef=tcdef;
      ip+=sizeof(struct NB_PLAN_ALIGN);
      }
    else{
      switch(*cursor){
        case 'y': step=&tcStepYear; break;
        case 'q': step=&tcStepQuarter; break;
        case 'n': step=&tcStepMonth; break;
        case 'w': step=&tcStepWeek; break;
        case 'd': step=&tcStepDay; break;
        case 'h': step=&tcStepHour; break;
        case 'm': step=&tcStepMinute; break;
        case 's': step=&tcStepSecond; break;
        default:
          // 2012-12-27 eat 0.8.13 - CID 751541 - count==0 is not possible - removed dead code
          snprintf(msg,msglen,"NB000E Expecting /\\?*{( or integer at \"%s\".",cursor);
          return(NULL);
        }
      cursor++;
      tmStep=(struct NB_PLAN_STEP *)ip;
      tmStep->op=(NB_PlanOp)&nbPlanStep;
      tmStep->count=count;
      tmStep->step=step;
      ip+=sizeof(struct NB_PLAN_STEP);
      }
    }
  else if(opt==0){      /* full blown plan syntax */
    /* check for state change symbol */
    if(symid=='='){
      tmWait=(struct NB_PLAN_WAIT *)ip;
      tmWait->op=(NB_PlanOp)&nbPlanWait;
      tmWait->offset=curstmt-source;
      ip+=sizeof(struct NB_PLAN_WAIT);
      tmValue=(struct NB_PLAN_VALUE *)ip;
      if(strcmp(ident,"=")==0) tmValue->op=(NB_PlanOp)&nbPlanValue; 
      else if(strcmp(ident,"==")==0) tmValue->op=(NB_PlanOp)&nbPlanDefine;
      else{
        snprintf(msg,msglen,"NB000E Unexpected relational operator - \"%s\".",ident);

        return(NULL);
        }
      if(NULL==(tmValue->value=nbParseCell((NB_Term *)context,&cursor,0))){
        sprintf(msg,"NB000E Syntax error in cell expression");
        return(NULL);
        }
      ip+=sizeof(struct NB_PLAN_VALUE);
      }
    else if(strcmp(ident,"on")==0 || strcmp(ident,"onif")==0){
      tmWait=(struct NB_PLAN_WAIT *)ip;
      tmWait->op=(NB_PlanOp)&nbPlanWait;
      tmWait->offset=curstmt-source;
      ip+=sizeof(struct NB_PLAN_WAIT);
      cursor++;
      tmCondEnable=(struct NB_PLAN_COND *)ip;
      if(strcmp(ident,"on")==0) tmCondEnable->op=(NB_PlanOp)&nbPlanOnEnable; 
      else tmCondEnable->op=(NB_PlanOp)&nbPlanWhenEnable;
      if(NULL==(tmCondEnable->cond=(NB_Cell *)nbParseCell((NB_Term *)context,&cursor,0))){
        sprintf(msg,"NB000E Syntax error in cell expression");
        return(NULL);
        }
      if(*cursor!=')'){
        snprintf(msg,msglen,"NB000E Expecting ')' at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      if(listInsertUnique(&(plan->objects),tmCondEnable->cond))
          grabObject(tmCondEnable->cond);
      ip+=sizeof(struct NB_PLAN_COND);
      tmCondTest=(struct NB_PLAN_COND *)ip;
      tmCondTest->op=(NB_PlanOp)&nbPlanWhenTest;
      tmCondTest->cond=tmCondEnable->cond;
      ip+=sizeof(struct NB_PLAN_COND);
      }
    else if(strcmp(ident,"if")==0){ /* if(..) <them> else <else> */
      while(*cursor==' ') cursor++;
      if(*cursor!='('){
        snprintf(msg,msglen,"NB000E Expecting '(' at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      tmIf=(struct NB_PLAN_IF *)ip;
      tmIf->op=(NB_PlanOp)&nbPlanIf;
      if(NULL==(tmIf->cond=(NB_Cell *)nbParseCell((NB_Term *)context,&cursor,0))){
        sprintf(msg,"NB000E Syntax error in cell expression");
        return(NULL);
        }
      if(*cursor!=')'){
        snprintf(msg,msglen,"NB000E Expecting ')' at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      while(*cursor==' ') cursor++;
      ip+=sizeof(struct NB_PLAN_IF);
      if(NULL==(ip=nbRuleParseStatement(context,opt,plan,ip,counter,source,&cursor,msg,msglen))) return(NULL);
      if(*cursor==';') cursor++; /* be flexible for the moment */
      tmIf->jump=ip-(char *)tmIf;  /* jump over {then} if false */
      symid=nbParseSymbol(ident,sizeof(ident),&cursor);
      if(strcmp(ident,"else")==0){ 
        tmBranch=(struct NB_PLAN_BRANCH *)ip;   /* Jump over else after doing then */
        tmBranch->op=(NB_PlanOp)&nbPlanBranch;
        ip+=sizeof(struct NB_PLAN_BRANCH);
        tmIf->jump=ip-(char *)tmIf;        /* jump over {then} plus jump if false */
        while(*cursor==' ') cursor++;
        if(NULL==(ip=nbRuleParseStatement(context,opt,plan,ip,counter,source,&cursor,msg,msglen))) return(NULL);
        if(*cursor==';') cursor++;  /* be flexible for the moment */
        tmBranch->jump=ip-(char *)tmBranch;
        }
      if(listInsertUnique(&(plan->objects),tmIf->cond)) grabObject(tmIf->cond);
      }
    else if(symid=='`'){  /* assertion */
      tmWait=(struct NB_PLAN_WAIT *)ip;
      tmWait->op=(NB_PlanOp)&nbPlanWait;
      tmWait->offset=curstmt-source;
      ip+=sizeof(struct NB_PLAN_WAIT);
      tmAssert=(struct NB_PLAN_ASSERT *)ip;
      tmAssert->op=(NB_PlanOp)&nbPlanAssert;
      if(NULL==(tmAssert->assertion=nbParseAssertion((NB_Term *)context,(NB_Term *)context,&cursor))){
        sprintf(msg,"NB000E Syntax error in assertion");
        return(NULL);
        }
      if(*cursor!=';'){
        snprintf(msg,msglen,"NB000E Expecting ';' at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      /* grab the assertion if we haven't already */
      for(member=tmAssert->assertion;member!=NULL;member=member->next){
        if(listInsertUnique(&(plan->objects),member->object)) grabObject(tmAssert->assertion);
        }
      ip+=sizeof(struct NB_PLAN_ASSERT);
      }
    else if(symid==':'){  /* command */
      tmWait=(struct NB_PLAN_WAIT *)ip;
      tmWait->op=(NB_PlanOp)&nbPlanWait;
      tmWait->offset=curstmt-source;
      ip+=sizeof(struct NB_PLAN_WAIT);
      cursor++;
      cursave=cursor;
      while(*cursor!=';' && *cursor!=0) cursor++;
      if(*cursor!=';'){
        snprintf(msg,msglen,"NB000E Command not terminated with ; at \"%s\"",cursave);
        return(NULL);
        }
      *cursor=0;
      tmCommand=(struct NB_PLAN_COMMAND *)ip;
      tmCommand->op=(NB_PlanOp)&nbPlanCommand;
      tmCommand->cmd=useString(cursave);
      if(listInsertUnique(&(plan->objects),tmCommand->cmd))
        grabObject(tmCommand->cmd);
      ip+=sizeof(struct NB_PLAN_COMMAND);
      *cursor=';';
      cursor++;
      }
    }

  /* consume a comma or semi-colon used as a delimiter for delays */
  while(*cursor==' ') cursor++;
  if(*cursor==',' || *cursor==';') cursor++;
  *cursorP=cursor;
  return(ip);
  }

static char *nbRuleParseBody(nbCELL context,int opt,NB_Plan *plan,char *ip,int counter,char *source,char **cursorP,char *msg,size_t msglen){
  char *cursor=*cursorP;
  while(*cursor!='}' && *cursor!=0){
    if(*cursor==',') cursor++;
    /* consider adding a parameter to enable a restricted syntax for unaligned schedules */
    ip=nbRuleParseStatement(context,opt,plan,ip,counter,source,&cursor,msg,msglen);
    if(ip==NULL){
      plan->object.next=(NB_Object *)nb_PlanFree;
      nb_PlanFree=plan;
      return(NULL);
      }
    }
  if(*cursor!='}'){
    sprintf(msg,"NB000E Unbalanced {}");
    return(NULL);
    }
  *cursorP=cursor;
  return(ip);
  }

/*
*  Parse a plan starting after the opening "{"
*/
static NB_Plan *nbRuleParsePlan(nbCELL context,int opt,char **source,char *msg,size_t msglen){
  char *cursor=*source;
  struct NB_PLAN_WAIT *tmWait;
  struct NB_PLAN_EXIT   *tmExit;
  char codebuf[NB_BUFSIZE];
  char *ip=codebuf;
  char savechar;
  size_t size;
  int counter=0;
  struct NB_PLAN *plan;

  while(*cursor==' ') cursor++;
  plan=newObject(nb_PlanType,(void **)&nb_PlanFree,sizeof(NB_Plan));
  plan->codeBegin=NULL;
  plan->codeEnd=NULL;
  plan->objects=NULL;
  plan->workspace=0;
  /* consider adding a parameter to enable a restricted syntax for unaligned schedules */
  ip=nbRuleParseBody(context,opt,plan,ip,counter,*source,&cursor,msg,msglen);
  if(ip==NULL){
    plan->object.next=(NB_Object *)nb_PlanFree;
    nb_PlanFree=plan;
    return(NULL);
    }
  if(opt==0){
    tmWait=(struct NB_PLAN_WAIT *)ip;
    tmWait->op=(NB_PlanOp)&nbPlanWait;
    tmWait->offset=cursor-*source;
    ip+=sizeof(struct NB_PLAN_WAIT);
    }
  cursor++;
  tmExit=(struct NB_PLAN_EXIT *)ip;
  tmExit->op=(NB_PlanOp)&nbPlanExit;
  ip+=sizeof(struct NB_PLAN_EXIT);
  size=ip-codebuf;
  plan->codeBegin=nbAlloc(size); // 2012-10-13 eat - replace malloc()
  plan->codeEnd=plan->codeBegin+size;
  memcpy(plan->codeBegin,codebuf,size);
  savechar=*cursor;
  *cursor=0;
  plan->source=grabObject(useString(*source));
  *cursor=savechar;
  grabObject(plan);
  *source=cursor;
  return(plan);
  }

NB_Rule *nbRuleParse(nbCELL context,int opt,char **source,char *msg,size_t msglen){
  NB_Rule *rule,**ruleP;
  NB_Term   *symContextSave=symContext;

  if(!(clientIdentity->authority&AUTH_DEFINE)){
    outMsg(0,'E',"Identity \"%s\" not authorized to execute rules.",clientIdentity->name->value);
    return(NULL);
    }
  if(NULL==(rule=nb_RuleFree)){
    rule=nbCellNew(nb_RuleType,NULL,sizeof(NB_Rule));
    rule->id=0;
    }
  else{
    nb_RuleFree=(NB_Rule *)rule->cell.object.next;
    rule->id++;
    }
  rule->localContext=grabObject(nbTermNew(NULL,"rule",nbNodeNew()));
  symContext=rule->localContext;
  if(NULL==(rule->plan=nbRuleParsePlan(context,opt,source,msg,msglen))){
    dropObject(rule->localContext);
    rule->cell.object.next=(NB_Object *)nb_RuleFree;
    nb_RuleFree=rule;
    symContext=symContextSave;
    return(NULL);
    }
  rule->identity=clientIdentity;
  rule->homeContext=grabObject(context);
  rule->time=nb_ClockTime;
  rule->cond=NULL;
  rule->valDef=nb_Unknown;
  rule->assertions=NULL;
  rule->command=NULL;
  rule->ip=(NB_PlanInstrP)rule->plan->codeBegin;
  rule->offset=0;
  /* counters not initialized */
  ruleP=nbRuleFind(rule);      /* link into active rule hash */
  rule->cell.object.next=(NB_Object *)*ruleP;
  *ruleP=rule;
  rule->state=NB_RuleStateRunning;  /* allow rule to execute */
  symContext=symContextSave;
  return(rule);
  }

NB_Rule *nbRuleExec(nbCELL context,char *source){
  NB_Rule *rule;
  char msg[1024];

  if(NULL==(rule=nbRuleParse(context,0,&source,msg,sizeof(msg)))){
    outPut("%s\n",msg);
    return(NULL);
    }
  grabObject(rule);   /* we'll drop it at the end of execution */
  outMsgHdr(0,'I',"");
  nbRuleShowItem(rule);
  outPut("\n");
  nbRuleEval(rule);
  return(rule);
  }

void nbRuleDouse(void){
  struct ACTION *action,*nextact;
  if(NULL!=(action=ashList)){
    ashList=NULL;
    for(;action!=NULL;action=nextact){
      nextact=action->nextAct;
      // Destroy actions from API or rule actions marked for destruction
      if(action->type=='A' || action->status=='D') destroyAction(action);
      else{
        action->nextAct=NULL;
        action->status='R';
        }
      }
    }
  }

/*
*  Schedule an action
*
*    This could be a more efficient structure if we have a large number of actions
*/
void scheduleAction(struct ACTION *action){
  struct ACTION *act,**actP=&actList;

  action->status='S';         /* scheduled */
  //for(act=*actP;act!=NULL && action->priority<=act->priority;act=*actP) actP=&(act->nextAct);
  for(act=*actP;act!=NULL && action->priority>act->priority;act=*actP) actP=&(act->nextAct);
  action->nextAct=*actP;
  *actP=action;
  if(trace) outMsg(0,'T',"scheduleAction insert action %p in list",action);
  }

struct ACTION *newAction(NB_Cell *context,NB_Term *term,struct COND *cond,char prty,void *assertion,NB_String *cmd,char option){
  struct ACTION *action;
  action=nbAlloc(sizeof(struct ACTION));
  // for some reason we are not using the action cell yet
  action->nextAct=NULL;
  // 2010-06-12 eat 0.8.2 - we don't grab the term in nbcmd.c when defining a rule, so we shouldn't grap it here
  //action->term=grabObjectNull(term);
  action->term=term;
  action->cond=grabObjectNull(cond);        // plug the condition pointer into the action 
  action->assert=assertion;
  // 2010-06-12 eat 0.8.2 - we don't grab the context in nbcmd.c when defining a rule, so we shouldn't grap it here
  //action->context=grabObjectNull(context);
  action->context=(struct NB_TERM *)context;
  action->command=grabObjectNull(cmd);      // action command is rest of line
  action->cmdopt=option|NB_CMDOPT_RULE;     // command option
  action->status='R';                       // ready 
  action->priority=prty;
  action->type='R';                         // assume Rule action
  return(action);
  }

void destroyAction(struct ACTION *action){
  // 2010-06-12 eat 0.8.2 - we didn't grab action->term - we shouldn't drop it
  //action->term=dropObjectNull(action->term);
  action->term=NULL;
  action->cond=dropObjectNull(action->cond);
  // 2010-06-12 eat 0.8.2 - we didn't grab action->context - we shouldn't drop it
  //action->context=dropObjectNull(action->context);
  action->context=NULL;
  action->command=dropObjectNull(action->command);
  action->assert=dropMember(action->assert);
  nbFree(action,sizeof(struct ACTION));
  }

void nbActionAssert(nbCELL context,nbSET assertion){
  struct ACTION *action;
  action=newAction(context,(NB_Term *)context,NULL,0,assertion,NULL,0);
  action->type='A';   // flag action from API
  scheduleAction(action);
  }

void nbActionCmd(nbCELL context,char *cmd,char option){
  struct ACTION *action;
  char *cursor=cmd;
  while(*cursor==' ') cursor++;
  if(*cursor==' ') return;
  action=newAction(context,(NB_Term *)context,NULL,0,NULL,useString(cursor),option);
  action->type='A';   // flag action from API
  scheduleAction(action);
  }

void nbAction(nbCELL context,nbSET assertion,char *cmd,char option){
  struct ACTION *action;
  char *cursor=cmd;
  while(*cursor==' ') cursor++;
  if(*cursor==0) action=newAction(context,(NB_Term *)context,NULL,0,assertion,NULL,option);   
  else action=newAction(context,(NB_Term *)context,NULL,0,assertion,useString(cursor),option);   
  action->type='A';   // flag action from API
  scheduleAction(action);
  }

void nbRuleAct(struct ACTION *action){
  struct COND *cond;
  int  savetrace=trace;
  char cmdopt;
  
  action->status='P';
  /* action can suppress sumbolic substition - context determines command echo option */
  cmdopt=action->cmdopt | ((NB_Node *)action->context->def)->cmdopt;
  /* if hushed, turn echo off, else if audit requested, turn echo on */
  if(cmdopt&NB_CMDOPT_HUSH) cmdopt&=255-NB_CMDOPT_ECHO;
  else if(nb_opt_audit) cmdopt|=NB_CMDOPT_ECHO;
  if(cmdopt&NB_CMDOPT_TRACE){
    trace=1;
    cmdopt|=NB_CMDOPT_ECHO; /* force echo with trace */
    }
  if(cmdopt&NB_CMDOPT_ECHO){
    outMsgHdr(0,'I',"Rule ");
    //termPrintName(action->term);
    nbTermPrintLongName(action->term);
    outPut(" fired ");
    if(action->assert!=NULL) printAssertedValues(action->assert);
    outPut("\n");
    }
  // 2007-07-22 eat 0.6.8 - modified to make sure the action command is issued before we react to action assertion in all cases
  if(action->assert!=NULL){
    if(action->cmdopt&NB_CMDOPT_ALERT){
      assert(action->assert,1);
      nbCellReact();
      contextAlert(action->context);
      }
    else assert(action->assert,0);
    if(action->command!=NULL)nbCmdSid((nbCELL)action->context,action->command->value,cmdopt,((NB_Node *)action->context->def)->owner);
    else nbRuleReact();  // react to changes - this is automatic with nbCmdSid   
    }
  else if(action->command!=NULL) nbCmdSid((nbCELL)action->context,action->command->value,cmdopt,((NB_Node *)action->context->def)->owner);
  /* undefine WHEN rules when they fire */
  cond=action->cond;
  if(cond!=NULL && cond->cell.object.type==condTypeWhenRule) termUndef(action->term);
  trace=savetrace;
  action->status='A';
  }

/*
*  React to state changes
*    This code is transitioning from the old rule structure to a new structure
*
*  This function is called after processing a single external input and after
*  processing a single second of scheduled activity.
*/
void nbRuleReact(void){
  struct ACTION *action,*nextact;
  NB_Rule *rule,*ready;
  NB_Term *symContextSave;

  nbCellReact();

  if((action=actList)!=NULL){
    actList=NULL;               /* start a new action list */     
    for(;action!=NULL;action=nextact){
      nextact=action->nextAct;      
      if(action->status=='S') nbRuleAct(action);
      action->nextAct=ashList;  // move to the ash list
      ashList=action;        
      }
    nbRuleDouse();
    }
  /*
  *  Perform rule actions
  *
  *    Allow rippling - but prevent any rule from firing more than once in this cycle
  */
  if((ready=nb_RuleReady)!=NULL){
    nb_RuleReady=NULL;  /* start new ready list */
    for(rule=ready;rule!=NULL;rule=rule->nextReady){
      if(rule->assertions!=NULL){
        outMsgHdr(0,'T',"%.8x.%.3d ",rule,rule->id);
        printAssertions(rule->assertions);
        outPut("\n");
        assert(rule->assertions,0);
        rule->assertions=NULL;
        }
      if(rule->command!=NULL){
        /* We are using the rules local context as the symbolic context */
        /* This allows local variables to be used in symbolc substitution: */
        /*    ${%a} at least, and perhaps %{a} */
        symContextSave=symContext;
        symContext=rule->localContext;
        outMsg(0,'T',"Rule %.8x.%.3d fired.",rule,rule->id);
        nbCmdSid((nbCELL)rule->homeContext,rule->command->value,1,rule->identity);
        symContext=symContextSave;
        rule->command=NULL;
        }
      if(rule->nextReady==NULL){
        nbCellReact();                 /* respond to any changes */
        rule->nextReady=nb_RuleReady;  /* append new ready list */
        nb_RuleReady=NULL;             /* start another ready list and continue */
        }
      }
    for(rule=ready;rule!=NULL;rule=rule->nextReady){
      rule->state=NB_RuleStateRunning;
      rule->cell.object.type->eval(rule);
      }
    }
  }

/*
*  Solve for rules in unknown state
*/
void nbRuleSolve(NB_Term *term){
  NB_Type *type;
  //NB_Term **termP;
  //struct HASH *hash;
  //long v;
  struct ACTION *action;
  struct COND *cond;
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;

  /* we insist on term->def having a non-NULL and valid value */
  /* definitions can be Unknown, but they should not be NULL */
  type=term->def->type;
  if(type==condTypeOnRule || type==condTypeWhenRule){
    cond=((struct COND *)term->def);
    if(cond->cell.object.value!=nb_Unknown) return;
    action=cond->right;
    if(trace){
      outPut("Solving: ");
      nbTermShowItem(term);
      }
    nbCellSolve_(cond->left);
    }   
  NB_TREE_ITERATE(treeIterator,treeNode,term->terms){
    term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
    nbRuleSolve(term);
    NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
    }
/*
  if(term->terms==NULL) return;
  if(term->terms->cell.object.type!=typeHash){
    for(term=term->terms;term!=NULL;term=(NB_Term *)term->cell.object.next) nbRuleSolve(term);
    }     
  else{
    hash=(struct HASH *)term->terms;
    termP=(NB_Term **)hash->vect;
    for(v=0;v<hash->modulo;v++){
      if(*termP!=NULL){
        for(term=*termP;term!=NULL;term=(NB_Term *)term->cell.object.next) nbRuleSolve(term);
        }  
      termP++;   
      }
    }
*/
  }  


/*
*  Object stuff
*/
void *nbRuleHash(struct HASH *hash,NB_Rule *rule){
  unsigned long h,*l;
  l=(unsigned long *)&rule;
  h=*l;
  return(&(hash->vect[h%hash->modulo]));
  }

NB_Rule **nbRuleFind(NB_Rule *rule){
  NB_Rule **ruleP;
 
  ruleP=nbRuleHash(nb_RuleType->hash,rule);
  for(;*ruleP!=NULL && *ruleP<rule;ruleP=(NB_Rule **)&((*ruleP)->cell.object.next));
  return(ruleP);
  }

/* print all rules */
void nbRuleShowAll(void){
  NB_Rule *rule,**ruleP;
  long v;
  long i;
  ruleP=(NB_Rule **)&(nb_RuleType->hash->vect);
  for(v=0;v<nb_RuleType->hash->modulo;v++){
    i=0;
    for(rule=*ruleP;rule!=NULL;rule=(NB_Rule *)rule->cell.object.next){
      outPut("H[%u,%ld]",v,i);
      outPut("R[%u]",rule->cell.object.refcnt); 
      outPut("L(%d) ",rule->cell.level);
      printObjectItem((NB_Object *)rule);
      outPut("\n");
      if(i<LONG_MAX) i++;
      }
    ruleP++; 
    }  
  }
 
void nbPlanPrint(NB_Plan *plan){
  outPut("%s\n",plan->source->value);
  } 

void nbRuleShowExpr(NB_Rule *rule){
  char delim,*cursor1,*cursor2;
  
  if(rule->plan==NULL){
    outPut("{...}");
    return;
    }
  cursor1=rule->plan->source->value;
  cursor2=cursor1+rule->offset;
  delim=*cursor2;
  *cursor2=0;
  outPut("{%s ^ ",cursor1);
  *cursor2=delim;
  outPut("%s",cursor2);
  }

void nbRuleShowItem(NB_Rule *rule){
  outPut("%8.8x.%3.3d = ",rule,rule->id);
  printObject(rule->cell.object.value);
  outPut(" == ");
  printObject((NB_Object *)rule);
  }

void destroyPlan(struct NB_PLAN *plan){
  dropMember(plan->objects);
  if(plan->codeBegin!=NULL){
    // 2012-10-13 eat - replaced free()
    nbFree(plan->codeBegin,plan->codeEnd-plan->codeBegin);
    plan->codeBegin=NULL;
    }
  plan->source=dropObject(plan->source);
  plan->object.next=(NB_Object *)nb_PlanFree;
  nb_PlanFree=plan;
  } 

void nbRuleDestroy(NB_Rule *rule){
  NB_Rule **ruleP;
  if(trace) outMsg(0,'T',"nbRuleDestroy() called");
  rule->plan=dropObject(rule->plan);
  rule->identity=NULL;  /* we don't grab or drop identities for now */
  rule->homeContext=dropObject(rule->homeContext);
  rule->localContext=dropObject(rule->localContext);
  ruleP=nbRuleFind(rule);
  if(*ruleP!=NULL) *ruleP=(NB_Rule *)rule->cell.object.next;
  rule->cell.object.next=(NB_Object *)nb_RuleFree;
  nb_RuleFree=rule;
  } 

/*
*   This routine executes instructions until a wait condition is returned.
*   A wait condition is indicated when an instruction returns a NULL
*   instruction pointer (ip).
*
*   We only execute instructions when a rule is in "running" state.  Otherwise
*   a rule behaves similar to a term by simply passing on the value of another
*   object.
*
*   This method may be invoked by:
*
*     1) Clock alarms - see nbRuleAlarm();
*     2) A change in the value of the "value object", if it is a cell we have
*        subscribed to.  We always return the value of the value object, so we
*        are covered here.      
*     3) A change in the value of a wait condition.  This may happen when we are
*        in a running state and have subscribed to a wait condition. It is up
*        to the rule plan to test the value of the condition.  This condition may
*        be unchanged when we are alerted by the "value object".
*
*   You will notice that a rule acts like a term that changes its definition
*   according to a plan.  In that respect, it always passes a value through from
*   the definition.  It has the added capability of making assertions and issuing
*   commands.
*/
NB_Object *nbRuleEval(NB_Rule *rule){
  NB_PlanInstrP ip=rule->ip;
  char *lowBound=rule->plan->codeBegin;
  char *highBound=rule->plan->codeEnd;

  if(rule->state==NB_RuleStateRunning){
    while((char *)(ip=(*(ip->op))(rule,ip))>=lowBound && (char *)ip<highBound);
    if(ip!=NULL || rule->state==NB_RuleStateStopped){
      if(ip!=NULL){
        outMsg(0,'L',"Corrupt plan detected by rule.");
        nbRuleShowItem(rule);
        outPut("\n");
        }
      dropObject(rule);
      }
    }
  return(rule->valDef->value);  /* always return value or the valDef object */
  }

/*
*  Clock alarm handler
*/
void nbRuleAlarm(NB_Rule *rule){
  rule->state=NB_RuleStateRunning;    /* return to running state */
  nbRuleEval(rule);
  }

void nbRuleEnable(NB_Rule *rule){
  //outMsg(0,'T',"nbRuleEnable() called");
  rule->state=NB_RuleStateRunning;
  nbCellEnable((NB_Cell *)rule->valDef,(NB_Cell *)rule);
  if(rule->cond!=NULL) nbCellEnable((NB_Cell *)rule->cond,(NB_Cell *)rule);
  if(rule->time>nb_ClockTime) nbClockSetTimer(rule->time,(NB_Cell *)rule);
  else nbRuleEval(rule);
  }

void nbRuleDisable(NB_Rule *rule){
  //outMsg(0,'T',"nbRuleDisable() called");
  rule->state=NB_RuleStateStopped;
  nbCellDisable((NB_Cell *)rule->valDef,(NB_Cell *)rule);
  if(rule->cond!=NULL) nbCellDisable((NB_Cell *)rule->cond,(NB_Cell *)rule);
  else if(rule->time>nb_ClockTime) nbClockSetTimer(0,(NB_Cell *)rule);
  }

void nbRuleInit(NB_Stem *stem){
  //nb_RuleHash=newHash(2031);
  nb_PlanType=newType(stem,"plan",NULL,0,nbPlanPrint,destroyPlan);
  nb_RuleType=newType(stem,"rule",NULL,0,nbRuleShowExpr,nbRuleDestroy);
  nb_RuleType->showItem=&nbRuleShowItem;
  nbCellType(nb_RuleType,NULL,nbRuleEval,nbRuleEnable,nbRuleDisable);
  nb_RuleType->alarm=&nbRuleAlarm;
  }
