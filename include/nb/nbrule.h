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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbrule.h 
* 
* Title:    Rule Object Header
*
* Function:
*
*   This header defines routines that implement rules.
*
* See nbrule.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-19 Ed Trettevik (split out in 0.4.1)
* 2003-10-31 eat 0.5.5  Started modifications to support rule procedures {...}
* 2004-09-25 eat 0.6.1  removed run list and added status
* 2005-05-15 eat 0.6.3  changed priority to signed char - not default on some platforms
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2014-01-27 eat 0.9.00 Include action.priorIf
*=============================================================================
*/
#ifndef _NB_RULE_H_
#define _NB_RULE_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>

// the ACTION structure may be simplified by converting all rules to the NB_RULE structure 
// We need to keep parts of it for the nbAction() API function
typedef struct ACTION{               /* rule function object */
  struct NB_CELL cell; 
  struct ACTION  *priorIf;   // prior if rule    // 2014-01-27 eat - added for performance
  struct ACTION  *nextAct;   // next rule (reactive) 
  struct NB_TERM *term;      /* rule term */
  struct COND    *cond;      /* rule condition */
  struct NB_LINK *assert;    /* rule assertion */
  struct NB_TERM *context;   /* rule command context */
  struct STRING  *command;   /* rule command text */
  char           cmdopt;     /* rule command option - or'd with context command option */
  char           status;     /* 'R' - ready, 'S' - scheduled, 'A' - ash (fired) */
                             /* 'D' - delete, 'E' - error, 'P' - processing  */
  signed char    priority;   /* action priority */
  char           type;       /* 'R' - rule, 'A' - API */
  } NB_Action;

NB_Action *newAction(NB_Cell *context,NB_Term *term,struct COND *cond,char prty,void *assertion,NB_String *cmd,char option);
void destroyAction(NB_Action *action);
void scheduleAction(NB_Action *action);

/*
*  A plan is the code executed by a rule (a rule is like a thread)
*  [We may switch to calling a plan a rule and a rule a thread.]
*/
struct NB_PLAN{
  struct NB_OBJECT object;
  struct STRING    *source;   /* source code */
  struct NB_LINK   *objects;  /* objects grabbed for code reference */
  int    workspace;        /* workspace bytes needed in thread */
  char   *codeBegin;       /* first byte of code buffer - first instruction */
  char   *codeEnd;         /* end of code buffer */
  };

typedef struct NB_PLAN NB_Plan;

struct NB_PLAN_INSTR{
    void *(*op)();
    };

typedef struct NB_PLAN_INSTR NB_PlanInstr;
typedef struct NB_PLAN_INSTR *NB_PlanInstrP;

#define NB_RuleStateRunning 0   /* Executing rule plan */
#define NB_RuleStateTimer   1   /* Waiting for clock alarm */
#define NB_RuleStateReady   2   /* Ready to take action */
#define NB_RuleStateStopped 3   /* Finished or disabled */

struct NB_RULE{
  NB_Cell         cell;
  struct NB_RULE *nextReady;    /* next ready rule */
  int             id;           /* wrap-around use counter */
  char            state;        /* see NB_RuleState... above */
  struct IDENTITY *identity;    /* client identity */
  NB_Term         *homeContext; /* home context - for normal terms */
  NB_Term         *localContext;/* local context - for thread specific terms */
  NB_Plan         *plan;
  NB_PlanInstr    *ip;
  int             offset;       /* offset into plan->source (for printing) */
  long            time;         /* time used for setting clock alarms */
  NB_Cell         *cond;        /* monitored condition - for enable/disable */
  NB_Object       *valDef;      /* value object - may be cell */
  NB_Link         *assertions;  /* ready assertions */
  NB_String       *command;     /* ready command */
  int             counter[10];  /* loop counters */
  };

typedef struct NB_RULE NB_Rule;
typedef struct NB_RULE *NB_RuleP;

typedef NB_PlanInstrP (*NB_PlanOp)(NB_RuleP sp,NB_PlanInstrP ip);





/**********************************************************
* Plan Instructions
**********************************************************/
/*
*  Flow control instructions
*/  
		
struct NB_PLAN_LOOP_BEGIN{
	NB_PlanOp op;
	int	      counter;	/* loop counter address - see counter array in state */
	int	      count;		/* Initial counter value */
	};

struct NB_PLAN_LOOP_END{
	NB_PlanOp op;
	int       counter;	/* loop counter offset */
  int       jump;     /* ip=ip+jump */
	};

struct NB_PLAN_BRANCH{
	NB_PlanOp op;
 	int       jump;     /* ip=ip+jump */
	};

struct NB_PLAN_COND{   /* This is used by multiple conditional instructions */
  NB_PlanOp op;
  NB_Cell *cond;       /* condition to operate on */ 
  };

struct NB_PLAN_IF{
  NB_PlanOp op;
  NB_Cell *cond;      /* condition to test */
  int       jump;     /* if cond==0 or cond==? then ip=ip+jump (false) */
                      /* We use the closed world assumption here */
  };

/*
*  Instructions that set a time for the next alert
*/

struct NB_PLAN_STEP{
	NB_PlanOp op;
	int	      count;		                   
	long      (*step)(long start,int count); 
	};

struct NB_PLAN_ALIGN{
	NB_PlanOp op;
	int	      count;    /* 0 - step to end of SCHED interval */
	tc        tcdef;                  
	};

struct NB_PLAN_WAIT{
	NB_PlanOp op;
  int       offset;
	};

/* 
*  Instructions that return values
*/

struct NB_PLAN_VALUE{
	NB_PlanOp  op;
	NB_Object  *value;
	};

struct NB_PLAN_EXIT{
	NB_PlanOp op;
	};

/*
*  Action Instructions
*/
struct NB_PLAN_ASSERT{
	NB_PlanOp op;
	struct NB_LINK *assertion;
	};

struct NB_PLAN_COMMAND{
  NB_PlanOp op;
  struct STRING *cmd;
  };

extern NB_Rule *nb_RuleReady;  /* Ready rule list - ready to take action */

/***********************************************/

void       nbRuleInit(NB_Stem *stem);
void       nbPlanPrint(struct NB_PLAN *plan);


void       nbPlanTest(char *source);

/***********************************************/

void       nbRuleShowExpr(struct NB_RULE *thread);
void       nbRuleShowItem(struct NB_RULE *thread);
void       nbRuleShowAll(void);
NB_Rule   **nbRuleFind(NB_Rule *thread);
NB_Rule   *nbRuleParse(NB_Cell *context,int opt,char **source,char *msg,size_t msglen);
//static NB_Plan   *nbRuleParsePlan(NB_Cell *context,int opt,char **source,char *msg,size_t msglen);
//static char *nbRuleParseBody(NB_Cell *context,int opt,NB_Plan *plan,char *ip,int counter,char *source,char **cursorP,char *msg,size_t msglen);
NB_Object *nbRuleEval(NB_Rule *thread);

NB_Object *nbRuleStep(NB_Rule *thread);
NB_Rule   *nbRuleExec(NB_Cell *context,char *source);

void       nbRuleDouse(void);

void       nbRuleSolve(NB_Term *term);

bfi tcPlan(long begin,long end,NB_Plan *plan,NB_Rule *thread);

#endif // NB_INTERNAL

//***************************************
// External Interface

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbActionAssert(nbCELL context,nbSET assertion);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbActionCmd(nbCELL context,char *cmd,char option);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbAction(nbCELL context,nbSET assertion,char *cmd,char option);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbRuleReact(void);

#endif
