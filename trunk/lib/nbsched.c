/*
* Copyright (C) 1998-2010 The Boeing Company
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
* File:     nbsched.c 
*
* Title:    NodeBrain Schedule Routines (prototype)
*
* Function:
*
*   This file provides routines that implement NodeBrain schedule
*   condition specification syntax and interpretation.  Like all
*   conditions, schedule conditions have values of true, false, unknown
*   and disabled.  The values of true and false are scheduled using
*   the syntax and algorithms defined here.
*
* Synospis:
*
*   #include "nb.h"
*
*   struct SCHEDULE *newSched(char *spec);
*   time_t schedStep(time_t floor,struct *SCHEDULE);
*
* Description
*
*   newSched()
*
*        Tranlate a schedule specification string and return a schedule
*        structure.  Schedules are hashed to avoid duplication. 
*        
*   schedNext()
*
*        Return the internal form (long) of the first time in a schedule
*        following the specified "floor" time.  For "delays" the floor
*        time is used as a starting point.         
*
*   
* Exit Codes:
*
*   0 - Successful completion
*   1 - Error (see message)
* 
*=============================================================================
* "Pulse" Time Condition Syntax:
*
*   ~<duration>
*
*    <duration> =: <number> <unit> [ <duration> ]
*    <unit>     =: w | d | h | m | s
*
*                  week, day, hour, minute, second
*
*   Examples:
*
*     ~30m
*     ~1h30m20s
*     ~2y
*     ~1w
*
* Calendar-Based Time Condition Syntax:
*
*   ~(...)   - see nbtime.h and related documentation
*
*   NOTE:  2002/07/23 eat
*
*   The old calendar-based time condition syntax is no longer supported.  There
*   are no known references to this syntax, but if any are found, they can be
*   converted to the new syntax easily.
*
*      ~n(1)d(5)h(4)m(2)   => ~(m(1/5@04:02)) -or- ~(m(2).h(4).d(5).n(1))
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 1999/04/05 Ed Trettevik (original prototype version)
* 2000/02/22 eat
*             1) Replace call of localtime_r with call to localtime.  This was
*                required to compile and link on a Sequent box that doesn't
*                have localtime_r. 
* 2001/07/13 eat - version 0.2.8
*             1) converted SCHED structure to the NB_Cell object scheme.
* 2002/07/23 eat - version 0.4.0
*             1) Started to incorporate new time condition syntax supported
*                by nbtime.h
*             2) Modified newSched to no longer require a null terminated
*                string.  It will return the delimiter location and let the
*                caller decide if it is appropriate for the context.  This 
*                will enable removal of some silly code in nodebrain's 
*                nbParseSymbol function where it tried to recognize the full 
*                schedule string and return it.
* 2002/07/31 eat - version 0.4.0
*             1) The old calendar based syntax has been dropped, but the old
*                "pulse" schedule syntax is still supported here.
*             2) switched back to expecting null delimited string from caller
*            
*             Note: This set of functions sets between the general condition
*                   parser and the nbtime.h routines.  The decisions about
*                   the type of schedule we are expecting ("delays" or 
*                   "repeating") should be made at the higher level.  The
*                   recognition of "pulse" schedules should be moved down to
*                   nbtime.h and this set of routines can probably merge
*                   into nbtime.h.
* 2003/03/15 eat 0.5.1  Split out nbsched.h and nbsched.c for make file
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *schedTypeTime;
struct TYPE *schedTypePulse;
struct TYPE *schedTypeDelay;

void *hashStr();
struct HASH *newHash();
struct STRING *useString();

struct PERIOD eternity;    /* 0 - maximum value */
  
/*  
* Note: Delay schedules must not contain state information 
*       because state will be relative to other conditions.
*       Other schedules "~", may contain state information 
*       which is common to all references. The period
*       structure contains state information, for example.
*/  

struct HASH *schedH;      /* hash of schedule entries */

/*
*  Schedule Functions
*/    
  
void schedPrintDump(struct SCHED *sched){
  outPut("%s schedule ~%u-%u ",sched->symbol->value,sched->period.start,
    sched->period.end);
  outPut("interval=%d,duration=%d)\n",sched->interval,sched->duration);
  }
      
/* change to printSched */
void schedPrint(struct SCHED *sched){
  outPut("%s",sched->symbol->value);
  } 

void destroySched(struct SCHED *sched){
  /*
  outMsg(0,'L',"This is where we would destroy a schedule.");
  */
  }  
   
  
/*
*  Initialize the schedule hash
*    Must be called before newSched
*/
void schedInit(NB_Stem *stem,long n){
  schedH=newHash(n);
  time(&eternity.start);
  eternity.end=0x7fffffff;  /* up to implementation limit */
  schedTypeTime=newType(stem,"~",schedH,0,schedPrint,destroySched);
  schedTypePulse=newType(stem,"~",schedH,0,schedPrint,destroySched);
  schedTypeDelay=newType(stem,"",schedH,0,schedPrint,destroySched);
  }

/*
*  Translate a schedule specification string into internal form.
*     The source string must be null terminated.
*/
struct SCHED *newSched(nbCELL context,char symid,char *source,char **delim,char *msg,int reuse){
  char *cursor,*curnum;
  char numstr[16];
  int  n;
  int r=1;  /* temp relation <0, 0, >0 */
  time_t interval;
  time_t duration;
  struct SCHED *sched,**schedP=NULL;
  struct TYPE *schedType;
  struct tcDef *tcdef;          /* Time condition definition */
  struct tcQueue *queue=NULL;   /* Time queue */
  long domainBegin;

  if(trace) printf("NB000T newSched \"%s\".\n",source);
  cursor=source;
  if(symid=='~'){
    if(*cursor>='0' && *cursor<='9') schedType=schedTypePulse;
    else schedType=schedTypeTime;
    }
  else if(symid=='T' || symid=='F' || symid=='U') schedType=schedTypeDelay;   /* delay */
  else{
    sprintf(msg,"NB000L newSched called with invalid symbol type code.");
    *delim=cursor;
    return(NULL);
    }

  /* see if we already have this schedule */
  if(reuse){
    schedP=hashStr(schedH,source);
    for(sched=*schedP;sched!=NULL && sched->cell.object.type==schedType && (r=strcmp(sched->symbol->value,source))>0;sched=(struct SCHED *)sched->cell.object.next){
      schedP=(struct SCHED **)&(sched->cell.object.next);
      }
    if(sched && r==0) return(sched);
    }
  
  interval=0;
  duration=0;
  if(schedType==schedTypeTime){  /* new syntax time conditions */
    if((tcdef=tcParse(context,&cursor,msg))==NULL){
      *delim=cursor;
      return(NULL);
      }
    domainBegin=tcTime();
    queue=tcQueueNew(tcdef,domainBegin,domainBegin+60);
    }
  else{  /* Pulse or Delay schedule */
    if(trace) printf("NB000T Assuming pulse schedule.\n");
    while(*cursor!=0 && strchr("0123456789",*cursor)!=NULL){
      curnum=numstr;
      while(*cursor!=0 && strchr("0123456789",*cursor)!=NULL){
        *curnum=*cursor;
        curnum++;
        cursor++;
        }
      *curnum=0;  
      n=atoi(numstr);
      if(trace) printf("NB000T n=%d c=%c\n",n,*cursor);
      if(*cursor=='s') interval+=n;
      else if(*cursor=='m') interval+=60*n;
      else if(*cursor=='h') interval+=3600*n;
      else if(*cursor=='d') interval+=86400*n;
      else if(*cursor=='w') interval+=704800*n;
      else{ 
        sprintf(msg,"NB000E expecting calendar code.");
        *delim=cursor;
        return(NULL);
        }
      cursor++;  
      }
    *delim=cursor;
    if(trace) printf("NB000T interval=%u\n",(unsigned int)interval);  
    if(cursor==source+1){
      sprintf(msg,"NB000E Expecting number or time function name.");
      return(NULL);
      }
    if(interval<2){
      sprintf(msg,"NB000E Intervals must be at least 2 second.");
      return(NULL);
      }  
    duration=interval; /* change to allow specification of duration */ 
    }
  sched=nbCellNew(schedType,NULL,sizeof(struct SCHED));
  if(reuse){
    sched->cell.object.next=(void *)*schedP;
    *schedP=sched;
    }
  sched->symbol=useString(source);
  sched->interval=interval;
  sched->duration=duration;
  if(trace) printf("NB000T nb_ClockTime=%u\n",(unsigned int)nb_ClockTime);
  if(schedType==schedTypePulse){
    sched->period.start=nb_ClockTime; /* Start right away */ 
    sched->period.end=sched->period.start+duration;
    }
  else{
    sched->period.start=0;
    sched->period.end=0;
    }
  sched->queue=queue;
  sprintf(msg,"NB000I Schedule translated successfully.");
  *delim=cursor;
  if(trace) schedPrintDump(sched);
  return(sched);         
  }
  
/*
*  Get the next time from a schedule
*/  
time_t schedNext(time_t floor,struct SCHED *sched){
  /* struct PERIOD *period; */
  time_t ceiling;

  if(floor==0) return(sched->period.end);
  if(sched->cell.object.type==schedTypePulse){
    while((sched->period.end=(sched->period.start+=sched->interval)+sched->duration)<=floor);
    return(sched->period.start);
    }
  if(sched->cell.object.type==schedTypeDelay) return(floor+sched->duration);
  if(sched->cell.object.type==schedTypeTime){
    if(sched->queue->tcdef->operation==tcPlan){
      /* Handle unaligned time procedures (plans) a bit different */
      /* This is only necessary when a time procedure stands alone */
      NB_Rule *rule=(NB_Rule *)sched->queue->tcdef->right;
      while(sched->period.start<floor && rule->state==NB_RuleStateRunning){
        sched->period.start=rule->time;
        nbRuleStep(rule);
        sched->period.end=rule->time;
        if(sched->period.end<sched->period.start){
          /* we exceeded the implementation limit on time value */
          return(maxtime);
          }
        }
      if(rule->valDef==NB_OBJECT_TRUE) return(sched->period.start);
      while(rule->state==NB_RuleStateRunning){
        sched->period.start=rule->time;
        nbRuleStep(rule);
        sched->period.end=rule->time;
        if(sched->period.end<sched->period.start){
          /* we exceeded the implementation limit on time value */
          return(maxtime);
          }
        if(rule->valDef==NB_OBJECT_TRUE) return(sched->period.start);
        }
      return(maxtime);
      }
    if(sched->period.end>floor) floor=sched->period.end;
    ceiling=floor+8*60*60;          /* 8 hour domain - is extended as necessary to find next interval */
    sched->period.start=tcQueueTrue(sched->queue,floor,ceiling);
    sched->period.end=tcQueueFalse(sched->queue);
    // 2012-10-12 eat - troubleshoot
    //tcPrintSeg(sched->period.start,sched->period.end,"Test"); 
    return(sched->period.start);
    }
  outMsg(0,'L',"schedNext() schedule type not recognized.");
  return(0);    
  }
