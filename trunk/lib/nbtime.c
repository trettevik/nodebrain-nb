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
* File:     nbtime.c
*
* Title:    Time Condition Routines
*
* Function:
*
*   This file provides functions that implement the NodeBrain specification
*   for time conditions.  The tcParse, tcDefine and tcCast
*   functions provide a method to incorporate time conditions into an
*   interpreter like NodeBrain.  The tcCast function "casts" a time condition
*   over some period of time, producing a "Binary Function of Integer" (bfi)
*   where the domain is time.  The casting algorithm depends on bfi routines
*   for time interval set manipulation. (See "nbbfi.c".)
*
* Synopsis:
*
*   #include "nb.h"
*
*   Primary Functions:
*   -----------------------------------------------------------------------------
*   tc  tcParse(nbCELL context,char **source,char *msg);
*   bfi tcCast(long begin,long end,tc condition);
*   int tcDefine(char *name,tc condition,char *msg);
*
*   Parsing Functions: (Used by tcParse)
*   -----------------------------------------------------------------------------
*   int tcParsePattern(int array[7],struct tcFunction *function, char **source, char *msg);
*   struct tcParm *tcParseParm(struct tcFunction *function, char **source, char *msg);
*   tc tcParseFunction(char **source,char *msg);
*   tc tcParseLeft(nbCELL conext,char **source,char *msg);
*
*   Time Function "Casting" Functions:
*   -----------------------------------------------------------------------------
*   bfi tcSimple(long begin,long end,struct tcFunction *left);
*   bfi tcComplex(long begin,long end,struct tcFunction *left,struct tcParm *right);
*
*   Time Operator "Casting" Functions: (see also bfi "casting" functions)
*   -----------------------------------------------------------------------------
*   bfi tcStretchStart(long begin,long end,tc left,tc right);   left is simple function 
*   bfi tcStretchStop(long begin,long end,tc left,tc right);    right is simple function 
*
*   bfi tcSelect(long begin,long end,tc left,tc right);
*   bfi tcReject(long begin,long end,tc left,tc right);
*   bfi tcIndex(long begin,long end,tc left,struct bfiIndex *right);
*
*   Binary Function of Integer (bfi) Infix "Casting" Functions:
*   -----------------------------------------------------------------------------
*   bfi tcAnd(long begin,long end,tc left,tc right);
*   bfi tcOr(long begin,long end,tc left,tc right);
*   bfi tcUnion(long begin,long end,tc left,tc right);
*   bfi tcUntil(long begin,long end,tc left,tc right);
*   bfi tcXor(long begin,long end,tc left,tc right);
*
*   Binary Function of Integer (bfi) Prefix "Casting" Functions: (left is NULL)
*   -----------------------------------------------------------------------------
*   bfi tcConflict(long begin,long end,tc left,tc right);
*   bfi tcConnect(long begin,long end,tc left,tc right);
*   bfi tcPartition(long begin,long end,tc left,tc right);
*   bfi tcNot(long begin,long end,tc left,tc right);
*   bfi tcOverlap(long begin,long end,tc left,tc right);
*   bfi tcNormalize(logn begin,long end,tc left,tc right);
*   bfi tcUnique(long begin,long end,tc left,tc right);
*
*   Alignment Functions: (closest boundary <= value given)
*   -----------------------------------------------------------------------------
*   long tcAlignYear(long timer, int unit);        year, decade, century, millennium
*   long tcAlignQuarter(long timer);               jan, apr, jul, oct
*   long tcAlignYearMonth(long timer,int month);   jan, feb, mar, ..., dec
*   long tcAlignMonth(long timer);                 1st day of month
*   long tcAlignWeek(long timer);                  sunday
*   long tcAlignWeekDay(long timer,int wday);      su, mo, tu, we, th, fr, sa
*   long tcAlignDay(long timer);                   @00:00:00
*   long tcAlignHour(long timer);                     :00:00
*   long tcAlignMinute(long timer);                      :00
*   long tcAlignSecond(long timer);                no change
*
*   Step Functions: (step one unit later than specified time)
*   -----------------------------------------------------------------------------
*   long tcStepMillennium(long timer);
*   long tcStepCentury(long timer);
*   long tcStepDecade(long timer);
*   long tcStepYear(long timer);
*   long tcStepQuarter(long timer);
*   long tcStepMonth(long timer);
*   long tcStepWeek(long timer);
*   long tcStepDay(long timer);
*   long tcStepHour(long timer);
*   long tcStepMinute(long timer);
*   long tcStepSecond(long timer);
* 
* Description:
*
*=============================================================================
* Note:
*
*   This concept was originally prototyped in Java in early 1999.  The C code
*   prototyped here is just a translation of the Java code to C.  However, we
*   are attempting to isolate the calendar functions from other scheduling
*   code.  We plan to make a corresponding change in the organization of the
*   Java version.
*
* Plan:
*
*   o Implement a set of routines to return the "next true" or "next false" time
*     for a time condition.  This will be used by NodeBrain to schedule the next
*     state change.
*   o Implement all operations defined in the NodeBrain specification for time
*     conditions.
*   o Include function to convert a tcDef tree to string. (tcString)
*   o Include function to free a tcDef tree.
*   o Consider including hashing to avoid duplication of tcDef, tcParm
*     and bfiIndex structures.  This would require usage counts or a decision
*     to retain tcDef trees indefinately.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2000/06/20 Ed Trettevik (started original C prototype version)
* 2000/07/29 eat
*             1) renamed to nbtime.h from nbcbs.h
*             2) renamed all calX and cbsX routines to tcX.
*             3) starting "next start" and "next stop" functionality to be used
*                by NodeBrain.
* 2002/08/11 eat - version 0.4.0
*             1) Modified tcQueueTrue to continue casting a schedule until
*                the next interval is found.  This eliminates a need to 
*                schedule a recast and the schedule queue will always display
*                the next interval.
* 2003/03/15 eat 0.5.1  Split into nbtime.h and nbtime.c for make file
* 2003/07/20 eat 0.5.4  Fixed bug in tcQueueTrue() causing interval skips
* 2003/10/27 eat 0.5.5  Fixed several bugs and deficiencies to conform to docs
* 2003/11/01 eat 0.5.5  Added time procedures (plan based time conditions)
* 2008/02/08 eat 0.6.9  Removed calendar hash
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-26 dtl Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=============================================================================
*/
#define _USE_32BIT_TIME_T
#include "nbi.h"
#include <limits.h>
#if defined(WIN32)
//#include <sys/timeb.h>
#endif

struct TYPE *nb_TimeCalendarType=NULL;
NB_Calendar *nb_TimeCalendarFree=NULL;
NB_Term   *nb_TimeCalendarContext=NULL;  /* calendar context */

extern int trace;

long maxtime=0x7fffffff; /* maximum time value */
long never=-1;           /* time we can't express */

void tcParmFree(struct tcParm *tcParm){
  struct tcParm *tcParmnext;
  while(tcParm!=NULL){
    tcParmnext=tcParm->next;
    //free(tcParm); // 2012-10-12 eat - switch to nbFree
    nbFree(tcParm,sizeof(struct tcParm));
    tcParm=tcParmnext;
    }
  }

/*************************************************************************
*   
*************************************************************************/  
/*
*  Convert time value to 34 byte string (33+null)
*/
char *tcTimeString(char *str,long timer){
  struct tm *timeTm;
  char *day[7];
  
  day[0]="su";
  day[1]="mo";
  day[2]="tu";
  day[3]="we";
  day[4]="th";
  day[5]="fr";
  day[6]="sa";
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) snprintf(str,34,"%s",".................................");
  else snprintf(str,34,"%s %4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d %10.10ld",day[timeTm->tm_wday],timeTm->tm_year+1900,timeTm->tm_mon+1,timeTm->tm_mday,timeTm->tm_hour,timeTm->tm_min,timeTm->tm_sec,timer);
  return(str);
  }
  
void tcPrintSeg(long start,long stop,char *label){
  char startStr[34],stopStr[34];
  
  tcTimeString(startStr,start);
  tcTimeString(stopStr,stop);
  outPut("%s - %s %s\n",startStr,stopStr,label);
  }
  
void tcPrintSet(bfi f,char *label){
  bfi s;

  outPut("=====================================================================\n");
  tcPrintSeg(f->end,f->start,label);  
  outPut("---------------------------------   ---------------------------------\n");
  for(s=f->next;s!=f;s=s->next){
    tcPrintSeg(s->start,s->end,"");
    }
  outPut("=====================================================================\n");
  } 

void tcPrintTime(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  outPut("%4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d\n",timeTm->tm_year+1900,timeTm->tm_mon+1,timeTm->tm_mday,timeTm->tm_hour,timeTm->tm_min,timeTm->tm_sec);
  }

long tcTime(void){
  time_t ltime;
  time(&ltime);
  return(ltime);
  }
  
/*
*  Alignment Routines:
* 
*    These routines enable a function pointer variable to control alignment
*    instead of having to test an alignment value.
*
*    Note: mktime will return a -1 if the time can not be expressed as an offest
*          from the Epoch.
*
*/
long tcAlignYear(long timer,int unit){
  struct tm *timeTm;

  /* unit: 1-year, 10-decade, 100-century, 1000-millennium */
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(unit!=1) timeTm->tm_year=(int)((timeTm->tm_year+1900)/unit)*unit-1900;
  timeTm->tm_mon=0;
  timeTm->tm_mday=1;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignQuarter(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(timeTm->tm_mon<3)      timeTm->tm_mon=0;
  else if(timeTm->tm_mon<6) timeTm->tm_mon=3;
  else if(timeTm->tm_mon<9) timeTm->tm_mon=6;
  else timeTm->tm_mon=9;
  timeTm->tm_mday=1;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignYearMonth(long timer,int month){
  struct tm *timeTm;
  
  fprintf(stderr,"tcAlignYearMonth called: %ld %d\n",timer,month);
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(timeTm->tm_mon>month) timeTm->tm_year++;
  // 2012-10-12 eat - can't be smaller than the minimum value
  //if((month-1)>=INT_MIN) timeTm->tm_mon=month-1; //dtl: added check
  timeTm->tm_mon=month-1; //eat
  timeTm->tm_mday=1;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignMonth(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  timeTm->tm_mday=1;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignWeek(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  timeTm->tm_mday-=timeTm->tm_wday;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignWeekDay(long timer,int wday){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  // 2012-10-12 eat - can't be greater than the maximum value
  //if(timeTm->tm_wday>wday && (wday+7)<=INT_MAX) wday=wday+7; //dtl: added check
  if(timeTm->tm_wday>wday) wday+=7; //eat
  timeTm->tm_mday+=wday-timeTm->tm_wday;
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcAlignDay(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  timeTm->tm_hour=0;
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  return(mktime(timeTm));
  }
  
long tcAlignHour(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  timeTm->tm_min=0;
  timeTm->tm_sec=0;
  return(mktime(timeTm));
  }

long tcAlignMinute(long timer){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  timeTm->tm_sec=0;
  return(mktime(timeTm));
  }

long tcAlignSecond(long timer){
  return(timer);
  }

/*
*  Step Routines
* 
*    These routines enable a function pointer variable to control stepping
*    instead of having to test a step value.
*
*    Note: If the step routines end up being called from only a couple places
*          the first and last lines should be moved to the caller.  These
*          routines should accept struct tm *timeTm as the parm.
*
*    n=0 is a duration request.  This will cause a difference for functions
*    like tcStepYearMonth and tcStepWeekDay.  For example,
*
*         tcStepYearMonth  n>0 (step Year) n=0 (step Month)
*         tcStepWeekDay    n>0 (step Week) n=0 (step Day)
*
*/
long tcStepMillennium(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_year+=n*1000;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

long tcStepCentury(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_year+=n*100;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

long tcStepDecade(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_year+=n*10;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

long tcStepYear(long timer,int n){
  struct tm *timeTm;
  
  fprintf(stderr,"tcStepYear called: timer=%ld n=%d\n",timer,n); 
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_year+=n;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcStepQuarter(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_mon+=n*3;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcStepMonth(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_mon+=n;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcStepWeek(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_mday+=n*7;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }
  
long tcStepDay(long timer,int n){
  struct tm *timeTm;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  if(n==0) n=1;
  timeTm->tm_mday+=n;
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

long tcStepHour(long timer,int n){return(timer+n*3600);}

long tcStepMinute(long timer,int n){return(timer+n*60);}

long tcStepSecond(long timer,int n){return(timer+n);}

/****************************************************************************
* Time Interval Set Functions
****************************************************************************/
/*
*  Cast a time condition over some period.
*/
bfi tcCast(long begin,long end,tc tcdef){
  bfi f,g;
  
  g=(tcdef->operation)(begin,end,tcdef->left,tcdef->right);
  f=bfiOre_(g);  /* normalize */
  bfiDispose(g);
  /* drop intervals from casting abnormalities - move this to cast */
  for(g=f->next;g!=f && g->end<=begin;g=g->next){
    g=bfiRemove(g);
    }
  return(f);
  }
   
/*
*  Table of time functions
*/
struct tcFunction{
  struct tcFunction *next;  /* Next table entry */
  char *name;               /* function name: e.g., "year", "month", "day", ... */  
  char *abbr;               /* abbreviated name: e.g., "y", "n", "d", ... */
  long (*align)();          /* alignment function - first interval */
  int  alignparm;           /* alignment parameter */
  long (*step)();           /* step function - step to next interval */
  long (*duration)();       /* duration function - step to stop time */
                              
                            /* Parameter interpretation values */
  int  unit;                /* 1=second,2=minute,3=hour,4=day,5=month,6=year, */
                            /* 7=decade,8=century,9=millennium,10=eternity    */
                            /* 0=special---selection within parent pattern */
  int  parent;              /* parent unit */
                            /* if parent=unit+2 (as with month of year functions) */
                            /* then alignparm provides the unit+1 (month) value   */
  long (*stepparent)();     /* Parent step function */
  };

struct tcFunction tcMillennium={NULL,"millennium","k",tcAlignYear,1000,tcStepMillennium,tcStepMillennium,9,10,NULL};
struct tcFunction tcCentury={&tcMillennium,"century","c",tcAlignYear,100,tcStepCentury,tcStepCentury,8,9,tcStepMillennium};      
struct tcFunction tcDecade={&tcCentury,"decade","e",tcAlignYear,10,tcStepDecade,tcStepDecade,7,8,tcStepCentury};      
struct tcFunction tcYear={&tcDecade,"year","y",tcAlignYear,1,tcStepYear,tcStepYear,6,7,tcStepDecade};      
struct tcFunction tcQuarter={&tcYear,"quarter","q",tcAlignQuarter,0,tcStepQuarter,tcStepQuarter,5,6,tcStepYear};      
struct tcFunction tcMonth={&tcQuarter,"month","n",tcAlignMonth,0,tcStepMonth,tcStepMonth,5,6,tcStepYear};      
struct tcFunction tcJan={&tcMonth,"january","jan",tcAlignYearMonth,1,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcFeb={&tcJan,"february","feb",tcAlignYearMonth,2,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcMar={&tcFeb,"march","mar",tcAlignYearMonth,3,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcApr={&tcMar,"april","apr",tcAlignYearMonth,4,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcMay={&tcApr,"may","may",tcAlignYearMonth,5,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcJun={&tcMay,"june","jun",tcAlignYearMonth,6,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcJul={&tcJun,"july","jul",tcAlignYearMonth,7,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcAug={&tcJul,"august","aug",tcAlignYearMonth,8,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcSep={&tcAug,"september","sep",tcAlignYearMonth,9,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcOct={&tcSep,"october","oct",tcAlignYearMonth,10,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcNov={&tcOct,"november","nov",tcAlignYearMonth,11,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcDec={&tcNov,"december","dec",tcAlignYearMonth,12,tcStepYear,tcStepMonth,4,6,tcStepYear};
struct tcFunction tcWeek={&tcDec,"week","w",tcAlignWeek,1,tcStepWeek,tcStepWeek,0,6,tcStepYear};      
struct tcFunction tcDay={&tcWeek,"day","d",tcAlignDay,0,tcStepDay,tcStepDay,4,5,tcStepMonth};      
struct tcFunction tcSu={&tcDay,"sunday","su",tcAlignWeekDay,0,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcMo={&tcSu,"monday","mo",tcAlignWeekDay,1,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcTu={&tcMo,"tuesday","tu",tcAlignWeekDay,2,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcWe={&tcTu,"wednesday","we",tcAlignWeekDay,3,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcTh={&tcWe,"thursday","th",tcAlignWeekDay,4,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcFr={&tcTh,"friday","fr",tcAlignWeekDay,5,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcSa={&tcFr,"saturday","sa",tcAlignWeekDay,6,tcStepWeek,tcStepDay,0,5,tcStepMonth};
struct tcFunction tcHour={&tcSa,"hour","h",tcAlignHour,0,tcStepHour,tcStepHour,3,4,tcStepDay};
struct tcFunction tcMinute={&tcHour,"minute","m",tcAlignMinute,0,tcStepMinute,tcStepMinute,2,3,tcStepHour};
struct tcFunction tcSecond={&tcMinute,"second","s",tcAlignSecond,0,tcStepSecond,tcStepSecond,1,2,tcStepMinute};

/*
*  Simple Functions:  align, step, and duration based operations.
*/
bfi tcSimple(long begin,long end,struct tcFunction *function){
  long start,stop;
  bfi f;

  f=bfiNew(begin,end);
  start=(function->align)(begin,function->alignparm);
  while(start<end){
    stop=(function->duration)(start,1);
    if(stop<=start) break;  /* we exceeded the implementation limit on time value */
    if(stop>begin) bfiInsertUnique(f,start,stop);
    if(function->step!=function->duration){
      start=(function->step)(start,1);
      if(start<0) break;  /* we exceeded the implementation limit on time value */
      }
    else start=stop;
    } 
  return(f); 
  }
 
/*
*  Plan based schedule
*/
bfi tcPlan(long begin,long end,NB_Plan *plan,NB_Rule *rule){
  long start,stop;
  bfi f;

  f=bfiNew(begin,end);
  start=begin;
  rule->time=start;
  rule->ip=(NB_PlanInstr *)plan->codeBegin;
  rule->state=1;
  rule->valDef=nb_Unknown;
  while(start<end && rule->state==1){
    start=rule->time;
    nbRuleStep(rule);
    stop=rule->time;
    if(stop<start) break;  /* we exceeded the implementation limit on time value */
    if(stop>begin && start<end && rule->valDef==NB_OBJECT_TRUE)
      bfiInsertUnique(f,start,stop);
    } 
  return(f); 
  }
 
/*
*  Trivial Function:  Implements the "i" function.
*/
bfi tcTrivial(long begin,long end){
  bfi f;
  
  f=bfiNew(begin,end);
  bfiInsert(f,begin,end);
  return(f);
  }
  
/*
*  Align to Parent Pattern - (see also tcAlignPattern)
*/  
long tcAlignParentPattern(long timer,int pat[]){
  struct tm *timeTm;
  int n,y;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  n=pat[0];
  if(n<0) n=-n;
  if(n>1){
    timeTm->tm_sec=0;
    if(n>2){
      timeTm->tm_min=0;
      if(n>3){
        timeTm->tm_hour=0;
        if(n>4){
          timeTm->tm_mday=1;
          if(n>5){
			timeTm->tm_mon=0;
            if(n>6){
              y=timeTm->tm_year+1900;
              switch(n){
                case 7: y=(int)(y/10)*10;
                case 8: y=(int)(y/100)*100;
                case 9: y=(int)(y/1000)*1000;
                }
              timeTm->tm_year=y-1900;    
              }
            }
          }
        }
      }
    }      
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

/*
*  Align to pattern - (see also tcCheckPattern)
*/  
long tcAlignPattern(long timer,int pat[],struct tcFunction *function){
  struct tm *timeTm;
  int n,y;
  long begin,end,start;
  
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(never);
  n=pat[0];
  if(n<0) n=-n;
  if(n>1){
    timeTm->tm_sec=pat[1];
    if(n>2){
      timeTm->tm_min=pat[2];
      if(n>3){
        timeTm->tm_hour=pat[3];
        if(n>4){
          timeTm->tm_mday=pat[4];
          if(n>5){
            timeTm->tm_mon=pat[5]-1;
            if(n>6){
              y=timeTm->tm_year+1900;
              switch(n){
                case 7:  y=(int)(y/10)*10+pat[6];     break;
                case 8:  y=(int)(y/100)*100+pat[6];   break;
                case 9:  y=(int)(y/1000)*1000+pat[6]; break;
                default: y=pat[6];
                }
              timeTm->tm_year=y-1900;
              }
            }
          }
        }
      }
    }      
  timeTm->tm_isdst=-1;
/*
  return(mktime(timeTm));
*/
  begin=mktime(timeTm);
  if((pat[7])==0) return(begin);
  /* handle special function alignment */
  end=(function->stepparent)(begin,1);
  if(end<0) end=maxtime;
  if(0>(start=(function->align)(begin,function->alignparm))) return(0);
  if(pat[7]>1) if(0>(start=(function->step)(start,pat[7]-1))) return(0);
  if(start>end) return(0);
  return(start);
  }
  
/*
*  Align stop pattern
*/  
long tcAlignStopPattern(long start,int pat[],struct tcFunction *function){
  struct tm *timeTm;
  int n,y;
  long stop;
  
  n=pat[0];
  if(n<0) n=-n;
  stop=tcAlignPattern(start,pat,function);
  /* handle special function (e.g. week and day of week) */
  if(pat[7]!=0){
    stop=(function->duration)(stop,1);
    if(stop<start){ /* rollover when stop pattern is less than start pattern */
      if(0>(stop=tcAlignParentPattern(start,pat))) return(never);
      if(0>(stop=(function->stepparent)(stop,1))) return(never);
      if(0>=(stop=tcAlignPattern(stop,pat,function))) return(never);
      if(0>(stop=(function->duration)(stop,1))) return(never);
      }
    return(stop);
    }
  /* handle normal functions that map to broken down time */
  if(stop<start){  /* rollover when stop pattern is less than start pattern */
    timeTm=localtime((const time_t *)&stop);
	if(timeTm==NULL) return(never);
    switch(n){
      case 6: timeTm->tm_year++; break;
      case 5: timeTm->tm_mon++;  break;
      case 4: timeTm->tm_mday++; break;
      case 3: timeTm->tm_hour++; break;
      case 2: timeTm->tm_min++;  break;
      case 1: timeTm->tm_sec++;  break;
      }
    timeTm->tm_isdst=-1;
    stop=mktime(timeTm);
    }
  timeTm=localtime((const time_t *)&stop);
  if(timeTm==NULL) return(never);
  if(n<5 || timeTm->tm_mday==pat[4]) return(stop);
  timeTm->tm_sec=pat[1];
  timeTm->tm_min=pat[2];
  timeTm->tm_hour=pat[3];
  timeTm->tm_mday=1;      /* select the first day of the new month */
  if(n>5){
    timeTm->tm_mon=pat[5]-1;
    if(n>6){
      y=timeTm->tm_year+1900;
      switch(n){
        case 7:  y=(int)(y/10)*10+pat[6];    break;
        case 8:  y=(int)(y/100)*100+pat[6];  break;
        case 9:  y=(int)(y/1000)*1000+pat[6]; break;
        default: y=pat[6];
        }            
      timeTm->tm_year=y-1900;
      }
    }
  timeTm->tm_isdst=-1;
  return(mktime(timeTm));
  }

/*
*  Check to see if a time matches a pattern
*
*  returns (0)false and (1)true
*/  
int tcCheckPattern(long timer,int pat[]){
  struct tm *timeTm;
  int n,y;
    
  timeTm=localtime((const time_t *)&timer);
  if(timeTm==NULL) return(0);
  n=pat[0];
  if(n>1){
    if(timeTm->tm_sec!=pat[1]) return(0);
    if(n>2){
      if(timeTm->tm_min!=pat[2]) return(0);
      if(n>3){
        if(timeTm->tm_hour!=pat[3]) return(0);
        if(n>4){
          if(timeTm->tm_mday!=pat[4]) return(0);
          if(n>5){
            if(timeTm->tm_mon!=pat[5]-1) return(0);
            if(n>6){
              y=timeTm->tm_year+1900;
              switch(n){
                case 7: y-=(int)(y/10)*10;     break;
                case 8: y-=(int)(y/100)*100;   break;
                case 9: y-=(int)(y/1000)*1000; break;
                }            
              if(y!=pat[6]) return(0);
              }
            }
          }
        }
      }
    } 
  return(1);  
  }
  
/*
*  Complex Function - Supports all functions with parameters
*/  
bfi tcComplex(long begin,long end,struct tcFunction *function,struct tcParm *parm){
  long (*duration)()=function->duration;
  long start,stop,parmstart,parmstop;
  struct tcParm *p;
  bfi f;

  f=bfiNew(begin,end);
  for(p=parm;p!=NULL;p=p->next){  /* for each parm */ 
    parmstart=tcAlignParentPattern(begin,p->start);
    while(parmstart>=0 && parmstart<end){         /* from begin to end */
      if(parmstart>0){ /* zero means special function could not align on this parent segment */   
        if(0>(start=tcAlignPattern(parmstart,p->start,function))) break; /* align and stop at limit */
        if(start<end && (function->unit==0 || tcCheckPattern(start,p->start))){
          if(0>(parmstop=tcAlignStopPattern(start,p->stop,function))) break;
          if(p->stop[0]>0){
            stop=parmstop;
            if(stop>begin) bfiInsertUnique(f,start,stop);
		        }
          else{
            if(parmstop>end) parmstop=end;      
            while(start<parmstop){
			        if(duration==&tcStepMonth && function->unit==4){ /* weird Jan,Feb,.. functions */
			          if(0>(stop=tcStepDay(start,1))) break;
                }
              else if(0>(stop=(duration)(start,1))) break;
              if(stop>begin){
				         bfiInsertUnique(f,start,stop);
                 }
              start=stop;
              }
            }
          }
        }
      if(p->step==NULL) break;
      if(0>(parmstart=(p->step)(parmstart,1))) break; /* step and stop if we hit limit */
      }
    } 
  return(f);   
  }


/*
*  Prefix Operations
*/
bfi tcPrefix(begin,end,left,right,op) long begin,end; tc left,right; bfi (*op)(); {
  bfi f,h;
  
  h=(right->operation)(begin,end,right->left,right->right);
  f=(op)(h);
  bfiDispose(h);
  return(f);
  }

bfi tcConflict(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiConflict_));
  }
bfi tcConnect(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiOr_));
  }

bfi tcPartition(begin,end,left,right) long begin,end; tc left,right; {
/* prototype expansion of domain for simple functions */
/* expand domain start as far as we might step forward */
/* expand domain stop as far as we might step backward */
/*  if(right->operation=tcSimple){  
    long Begin=begin,End=end;
    struct tcFunction *function;
    bfi f,s;
    
    function=((struct tcFunction *)((tc)right)->left);  
    Begin=(function->step)(begin,-1);
    End=(function->step)(end,1);
    f=tcPrefix(Begin,End,left,right,bfiUntil_);
    for(s=f->next;s!=f;s=s->next){
      if(s->end<begin || s->start>=end) s=bfiRemove(s);
      }
    return(f);
    }*/ 
  return(tcPrefix(begin,end,left,right,bfiUntil_));
  }
bfi tcNot(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiNot_));
  }
bfi tcOverlap(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiAnd_));
  }
bfi tcNormalize(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiOre_));
  }
/* 2003/10/27 eat - switched to edge preservation XOR */
bfi tcUnique(begin,end,left,right) long begin,end; tc left,right; {
  return(tcPrefix(begin,end,left,right,bfiXore_));
  }
 
/*
*  Special Infix Operations
*/
bfi tcIndex(begin,end,left,right) long begin,end; tc left; struct bfiindex *right; {
  bfi f,g;

  g=(left->operation)(begin,end,left->left,left->right);
  f=bfiIndex(g,right);
  bfiDispose(g);
  return(f);
  }

/*
*  Selection
*/    
bfi tcSelect(begin,end,left,right) long begin,end; tc left,right; {
  bfi f,s,t,g,h;
  
  /* tcPrintSeg(begin,end,"Select called"); */
  f=bfiNew(begin,end);
  h=(right->operation)(begin,end,right->left,right->right);
  for(s=h->next;s!=h;s=s->next){
    /* tcPrintSeg(s->start,s->end,"Select left"); */
    g=(left->operation)(s->start,s->end,left->left,left->right);
    for(t=g->next;t!=g;t=t->next){
      if(t->start<end && t->end>begin) bfiInsertUnique(f,t->start,t->end);
      }
    bfiDispose(g);
    }
  bfiDispose(h);
  return(f);
  }

/*
*  Infix Rejection
*
*    When computing g!h, we first compute g over the specified domain
*    and then compute h over a domain adjusted to the earliest start and
*    the latest stop of g.  This ensures that we have all the h intervals
*    that can possibly intersect with g intervals and cause rejection.
*/  
bfi tcReject(begin,end,left,right) long begin,end; tc left,right; {
  bfi f,g,h;

  g=(left->operation)(begin,end,left->left,left->right);
  if(g->next==g) return(g);  /* g is empty */
  f=g->prior;   /* find interval with largest end time */
  for(h=g->prior->prior;h!=g;h=h->prior) if(h->end>f->end) f=h;
  /* Get the rejection set over a domain adjusted to g's intervals. */
  h=(right->operation)(g->next->start,f->end,right->left,right->right);
  if(h->next==h){     /* don't bother if rejection set is empty */
    bfiDispose(h);
    return(g);
    }
  h->start=g->start;  /* use original domain for bfiReject */
  h->end=g->end;
  f=bfiReject(g,h);
  bfiDispose(g);
  bfiDispose(h);
  return(f);
  }

/*
*  Stretch Start
*/
bfi tcStretchStart(begin,end,left,right) long begin,end; tc left,right;{
  bfi f,s,h;
  struct tcFunction *function;
  long start,End;
  
  function=((struct tcFunction *)((tc)left)->left);
  f=bfiNew(begin,end);
  End=(function->step)(end,1);  /* expand domain end as far as we might step back */
  h=(right->operation)(begin,End,right->left,right->right);
  for(s=h->next;s!=h;s=s->next){
    start=(function->align)(s->start,function->alignparm);
    if(start>s->start) start=(function->step)(start,-1);
    if(start>=end) break;
    bfiInsertUnique(f,start,s->end);
    }
  bfiDispose(h);
  return(f);
  }

/*
*  Stretch Stop
*/
bfi tcStretchStop(begin,end,left,right) long begin,end; tc left,right;{
  bfi f,s,g;
  struct tcFunction *function;
  long stop,Begin;
  
  function=((struct tcFunction *)((tc)right)->left);  
  f=bfiNew(begin,end);
  Begin=(function->step)(begin,-1);   /* expand domain begin as far as we might step forward */
  g=(left->operation)(Begin,end,left->left,left->right);
  for(s=g->next;s!=g;s=s->next){
    stop=(function->align)(s->end,function->alignparm);
    if(stop<s->end) stop=(function->step)(stop,1);
    if(stop>begin) bfiInsertUnique(f,s->start,stop);
    }
  bfiDispose(g);
  return(f);
  }
  
/*
*  Simple Infix Operations
*/  
bfi tcInfix(begin,end,left,right,op) long begin,end; tc left,right; bfi (*op)(); {
  bfi f,g,h;
  
  g=(left->operation)(begin,end,left->left,left->right);
  h=(right->operation)(begin,end,right->left,right->right);
  f=(op)(g,h);
  bfiDispose(g);
  bfiDispose(h);
  return(f);
  }
bfi tcAnd(begin,end,left,right) long begin,end; tc left,right; {
  return(tcInfix(begin,end,left,right,bfiAnd));
  }
bfi tcOr(begin,end,left,right) long begin,end; tc left,right; {
  return(tcInfix(begin,end,left,right,bfiOr));
  }
bfi tcUnion(begin,end,left,right) long begin,end; tc left,right; {
  return(tcInfix(begin,end,left,right,bfiUnion));
  }
bfi tcUntil(begin,end,left,right) long begin,end; tc left,right; {
  return(tcInfix(begin,end,left,right,bfiUntil));
  }
bfi tcXor(begin,end,left,right) long begin,end; tc left,right; {
  return(tcInfix(begin,end,left,right,bfiXor));
  }

/**************************************************************************
* Schedule Expression Parsing Routines
**************************************************************************/  

int tcParseTime(start,stop,source,msg) long *start,*stop; char **source,*msg; {
  char *cursor=*source,*number,delim;
  struct tm timer;

  number=cursor;
  while(*cursor>='0' && *cursor<='9') cursor++;
  if(cursor==number){
    sprintf(msg,"NB000E Expecting integer value at \"%s\".",cursor);
    return(0);
    }
  if(*cursor=='#'){   /* Expressed as integer value */ 
    delim=*cursor;
    *cursor=0;
    *start=atoi(number);
    *cursor=delim;
    *stop=*start+1;
    *source=cursor+1;
    return(1);
    }  
  /* initialize timer */
  timer.tm_mon=0;
  timer.tm_mday=1;
  timer.tm_hour=0;
  timer.tm_min=0;
  timer.tm_sec=0;
  timer.tm_isdst=-1;  
  /* year */
  delim=*cursor;
  *cursor=0;
  timer.tm_year=atoi(number)-1900;
  *cursor=delim;
  if(delim!='/'){
    *start=mktime(&timer);
    timer.tm_year++;
    }
  else{  /* month */
    cursor++;
    number=cursor;
    while(*cursor>='0' && *cursor<='9') cursor++;
    delim=*cursor;
    *cursor=0;
    timer.tm_mon=atoi(number)-1;
    *cursor=delim;
    if(delim!='/'){
      *start=mktime(&timer);
      timer.tm_mon++;
      }
    else{  /* day */
      cursor++;
      number=cursor;
      while(*cursor>='0' && *cursor<='9') cursor++;
      delim=*cursor;
      *cursor=0;
      timer.tm_mday=atoi(number);
      *cursor=delim;
      if(delim!='@'){
        *start=mktime(&timer);
        timer.tm_mday++;
        }
      else{ /* hour */
        cursor++;
        number=cursor;
        while(*cursor>='0' && *cursor<='9') cursor++;
        delim=*cursor;
        *cursor=0;
        timer.tm_hour=atoi(number);
        *cursor=delim;
        if(delim!=':'){
          *start=mktime(&timer);
          timer.tm_hour++;
          }
        else{ /* minute */
          cursor++;
          number=cursor;
          while(*cursor>='0' && *cursor<='9') cursor++;
          delim=*cursor;
          *cursor=0;
          timer.tm_min=atoi(number);
          *cursor=delim;
          if(delim!=':'){
            *start=mktime(&timer);
            timer.tm_min++;
            }
          else{ /* second */
            cursor++;
            number=cursor;
            while(*cursor>='0' && *cursor<='9') cursor++;
            delim=*cursor;
            *cursor=0;
            timer.tm_sec=atoi(number);
            *cursor=delim;
            *start=mktime(&timer);
            timer.tm_sec++;
            }
          }
        }
      }
    }
  timer.tm_isdst=-1;
  *stop=mktime(&timer);
  *source=cursor;
  return(1);
  }

int tcParseSegment(start,stop,source,msg) long *start,*stop; char **source,*msg; {
  long whatever;

  if(!tcParseTime(start,stop,source,msg)) return(0);
  if(**source=='-'){
    (*source)++;
    if(!tcParseTime(stop,&whatever,source,msg)) return(0);
    }
  else if(**source=='_'){
    (*source)++;
    if(!tcParseTime(&whatever,stop,source,msg)) return(0);
    }
  return(1);
  }

/*
*  Parse calendar function parameter pattern
*
*  Syntax:
*
*    <pattern>   := <number> [ <separator> <pattern> ]
*    <number>    := <digit> [ <number> ]
*    <digit>     := 0-9
*    <separator> := / | @ | : | . 
*
*  Examples:
*
*    2000/6/5   - valid within the context of "day"
*    5/25@8:25  - valid within the context of "minute"
*     
*/
int tcParsePattern(int *array,struct tcFunction *function,char **source,char *msg){
  char *cursor=*source,pattern[60],*delimiter;
  char *patcur=pattern;
  int i,n=function->unit,special=0;

  for(i=0;i<8;i++) *(array+i)=0;  /* init array */
  if(n==0){
	  n=function->parent-1;
	  special=n;
  }
  while((*cursor>='0' && *cursor<='9') || *cursor=='/' || *cursor==':' || *cursor=='@'){
    *patcur=*cursor;
    patcur++;
    cursor++;
    }
  *source=cursor;
  *patcur=0;
  if(n>6) n=6;
  if(n>4) array[4]=1;
  if(n>5) array[5]=1;  /* note: we could convert values at parse time */
  delimiter=" ::@//";  
  while(patcur>pattern){
    if(n>6){
      sprintf(msg,"NB000E Too many parent parameters in calendar function parameter. \"%s\"",pattern);
      return(0);
      }
    patcur--;
    while(patcur>=pattern && *patcur>='0' && *patcur<='9') patcur--;
    if(patcur>=pattern){    
      if(*(delimiter+n)!=*patcur && '.'!=*patcur){
        sprintf(msg,"NB000E Unexpected separator in calendar function parameter. \"%s\"",pattern);
        return(0);
        }
      *patcur=0;
      }
    if(n==special){  /* special functions like week, su, mo, tu, ... */
	  *(array+7)=atoi(patcur+1);
	  if(n==4 || n==5) *(array+n)=1;
	  else *(array+n)=0;
	}
	else{
	  *(array+n)=atoi(patcur+1);
	  if(function==&tcQuarter && n==5) *(array+n)=(*(array+n)-1)*3+1; /* adjust from month to quarter */
	}
    n++;
    if(n<function->parent){
      *(array+n)=function->alignparm;
      n++;
      }
    }
  if(n==7){
    i=strlen(patcur+1);
    if(i>3) n=10;
    else n=6+i;
    } 
  *array=n;
  return(1);
  }

/*
*  Parse Time Function Parameter List
*
*  Syntax:
*
*    <parmlist> := ( <parmbody> )
*    <parmbody> := argument [ , <parmbody> ]
*    <argument> := <pattern> | <pattern>_<pattern> | <pattern>..<pattern>
*/
struct tcParm *tcParseParm(struct tcFunction *function,char **source,char *msg){
  struct tcParm *tcParm,*tcParmnext=NULL;
  char *cursor;
  int i;

  if(**source!='('){
    sprintf(msg,"NB000L Expecting left parenthesis at \"%s\"",*source);
    return(NULL);
    }
  (*source)++;
  while(1){
    //if((tcParm=(struct tcParm *)malloc(sizeof(struct tcParm)))==NULL) //2012-01-26 dtl: handled out of memory
    //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
    tcParm=(struct tcParm *)nbAlloc(sizeof(struct tcParm));
    tcParm->next=tcParmnext;        
    if(!tcParsePattern(tcParm->start,function,source,msg)){
      tcParmFree(tcParm);
      return(NULL);
      }
    cursor=*source;
    if(*cursor=='.' && *(cursor+1)=='.'){  /* handle range */
      (*source)+=2;
      if(!tcParsePattern(tcParm->stop,function,source,msg)){
        tcParmFree(tcParm);
        return(NULL);
        }
      tcParm->stop[0]=-tcParm->stop[0]; /* flag range with negative count */
      } 
    else{
      if(*cursor=='_'){                  /* handle span */
        (*source)++;
        if(!tcParsePattern(tcParm->stop,function,source,msg)){
          tcParmFree(tcParm);
	      return(NULL);
          }
        }
      else{                                /* handle simple pattern */
        for(i=0;i<8;i++) tcParm->stop[i]=tcParm->start[i];
	}
      }	
	if(function==&tcQuarter) tcParm->stop[function->unit]+=3;
 	else if(function->unit!=0) tcParm->stop[function->unit]++;               /* step to end of last unit */
    switch(tcParm->start[0]){
      case 1: tcParm->step=tcStepSecond;    break;
      case 2: tcParm->step=tcStepMinute;    break;
      case 3: tcParm->step=tcStepHour;      break;
      case 4: tcParm->step=tcStepDay;       break;
      case 5: tcParm->step=tcStepMonth;     break;
      case 6: tcParm->step=tcStepYear;      break;
      case 7: tcParm->step=tcStepDecade;    break;
      case 8: tcParm->step=tcStepCentury;   break;
      case 9: tcParm->step=tcStepMillennium; break;
      case 10: tcParm->step=NULL; break;
      }
    if(**source!=',') break;
    (*source)++;
    tcParmnext=tcParm;
    }
  if(**source!=')' && **source!=0){
    tcParmFree(tcParm);
    sprintf(msg,"NB000E Expecting comma ',' or right parenthesis ')' at \"%s\"",*source);
    return(NULL);
    }
  (*source)++;
  return(tcParm);
  }

/*
*  Parse Time Function Call
*
*  o Quarter,Week,Su-Sa are not currently supported by the parameter notation.
*/   
tc tcParseFunction(char **source,char *msg){
  tc tcdef;
  char *cursor,*name=*source,mark;
  bfi (*operation)()=tcSimple;
  void *right=NULL;
  struct tcFunction *function=NULL;
  NB_Term *term;

  /* get the name - restricted identifier */
  for(cursor=*source;(*cursor>='a' && *cursor<='z') || (*cursor>='A' && *cursor<='Z') || (*cursor>='0' && *cursor<='9');cursor++);
  mark=*cursor;
  *cursor=0;
  if(*name>='A' && *name<='Z'){  /* user declared time expression */
    if((term=nbTimeLocateCalendar(name))==NULL){
      *cursor=mark;   /* repare the source */
      snprintf(msg,(size_t)NB_MSGSIZE,"NB000E Time function \"%s\" not declared.",name); //dtl used snprintf
      return(NULL);  /* return without updating the source pointer */
      }
    *cursor=mark;   /* repare the source */
    *source=cursor;
    return(((NB_Calendar *)term->def)->tcdef);
    }
  if(*name=='i' && *(name+1)==0){
    operation=tcTrivial;
    *cursor=mark;
    *source=cursor;
    }

  else{  /* lookup the name as a built-in time function */
    for(function=&tcSecond;function!=NULL && strcmp(name,function->name)!=0 && strcmp(name,function->abbr)!=0;function=function->next);
    *cursor=mark;     /* repair the source */
    if(function==NULL){
      *cursor=mark;   /* repare the source */
      sprintf(msg,"NB000E Time function \"%s\" not recognized.",name);
      return(NULL);   /* return without updating the source pointer */
      }
    *source=cursor;   /* update the source pointer */
    if(mark=='('){    /* get parameters */
      operation=tcComplex;
      if((right=tcParseParm(function,source,msg))==NULL) return(NULL);
      }
    }
  /* build a tcDef structure */
  //if((tcdef=(tc)malloc(sizeof(struct tcDef)))==NULL) //2012-01-26 dtl: handled out of memory
  //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
  tcdef=(tc)nbAlloc(sizeof(struct tcDef));
  tcdef->operation=operation;
  tcdef->left=function;
  tcdef->right=right;  
  return(tcdef);    
  }
  
tc tcParseLeft(nbCELL context,char **source,char *msg){
  tc tcdef,left=NULL,right;
  char *cursor=*source;
  bfi (*operation)();

  if((*cursor>='a' && *cursor<='z') || (*cursor>='A' && *cursor<='Z'))
    return(tcParseFunction(source,msg));
  if(*cursor=='('){
    cursor++;
    if((left=tcParse(context,&cursor,msg))==NULL) return(NULL);
    if(*cursor!=')'){
      sprintf(msg,"NB000E Expecting right parenthesis.");
      *source=cursor; /* return the cursor */
      /* free left ? */
      return(NULL);
      }
    cursor++;
    *source=cursor;
    return(left);
    }
  if(*cursor=='{'){  /* plan? */
    cursor++;
    if((right=(void *)nbRuleParse(context,1,&cursor,msg))==NULL) return(NULL);
    //if((tcdef=(tc)malloc(sizeof(struct tcDef)))==NULL) //2012-01-26 dtl: handled out of memory
    //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
    tcdef=(tc)nbAlloc(sizeof(struct tcDef)); 
    tcdef->operation=tcPlan;
    tcdef->left=((NB_Rule *)right)->plan;
    tcdef->right=right;
    *source=cursor;   /* return the cursor */  
    return(tcdef);
    }
  /*
  *  Check for prefix operators
  */
  if(*cursor=='=') operation=tcConflict;         /* segments that overlap */
  else if(*cursor=='!') operation=tcNot;         /* not */
  else if(*cursor=='&') operation=tcOverlap;     /* and */
  else if(*cursor=='|') operation=tcConnect;     /* or */
  else if(*cursor=='~') operation=tcNormalize;   /* or, preserving edges */
  else if(*cursor=='%') operation=tcUnique;      /* xor */
  else if(*cursor=='#') operation=tcPartition;   /* until */
  else if(*cursor=='_') operation=tcPartition;   /* until */
  else{
    sprintf(msg,"NB000E Time condition prefix operator \"%c\" not recognized.",*cursor);
    *source=cursor; /* return the cursor */
    return(NULL);
    }
  /*
  *  We found a prefix operator
  */
  cursor++;
  if((right=tcParseLeft(context,&cursor,msg))==NULL){
    /* free left ? */
    *source=cursor;
    return(NULL);
    }
  /* build a tcDef structure */
  //if((tcdef=(struct tcDef *)malloc(sizeof(struct tcDef)))==NULL) //2012-01-26 dtl: handled out of memory
  //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
  tcdef=(struct tcDef *)nbAlloc(sizeof(struct tcDef));
  tcdef->operation=operation;
  tcdef->left=left;
  tcdef->right=right;
  *source=cursor;   /* return the cursor */  
  return(tcdef);    
  }
    
tc tcParse(nbCELL context,char **source,char *msg){
  tc tcdef,left=NULL;
  char *cursor=*source,*index;
  bfi (*operation)();
  void *right;
  char indexMsg[512];

  if((left=tcParseLeft(context,&cursor,msg))==NULL){
    *source=cursor;
    return(NULL);
    }
  /*
  *  Check for infix operators
  */
  if(*cursor=='<')      operation=tcStretchStart;
  else if(*cursor=='>') operation=tcStretchStop;
  else if(*cursor==',') operation=tcUnion;
  else if(*cursor=='=') operation=tcSelect;
  else if(*cursor=='.') operation=tcSelect;
  else if(*cursor=='!') operation=tcReject;
  else if(*cursor=='#') operation=tcUntil;
  else if(*cursor=='_') operation=tcUntil;
  else if(*cursor=='&') operation=tcAnd;
  else if(*cursor=='|') operation=tcOr;
  else if(*cursor=='%') operation=tcXor;
  else if(*cursor=='['){  /* indexed selection */
    operation=tcSelect;
    index=cursor+1;
    if((cursor=strchr(index,']'))==NULL){
      sprintf(msg,"NB000E Expecting ']' terminating index.");
      *source=index;
      return(NULL);
      }
    *cursor=0;
    /*
    *  Note: need to change bfiIndexParse to conform to tcParse routines
    *          right=bfiIndexParse(&index,msg);
    */
    if((right=bfiIndexParse(index,indexMsg,sizeof(indexMsg)))==NULL){
      sprintf(msg,"NB000E Invalid index \"%s\". %s",index,indexMsg);
      *source=cursor;
      *cursor=']';
      return(NULL);
      }
    *cursor=']';
    /* build a tcDef structure for index operation */
    //if((tcdef=(tc)malloc(sizeof(struct tcDef)))==NULL) //2012-01-26 dtl: handled out of memory
    //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
    tcdef=(tc)nbAlloc(sizeof(struct tcDef));
    tcdef->operation=tcIndex;
    tcdef->left=left;
    tcdef->right=right;
    left=tcdef;
    }
  else{
    *source=cursor; /* return the cursor */
    return(left);   /* leave it to the caller to check delimiter */
    }
  /*
  *  We found an infix operator
  */
  cursor++;
  if((right=tcParse(context,&cursor,msg))==NULL){
    /* free left ? */
    *source=cursor;  /* return the cursor */
    return(NULL);
    }
  /*
  *  Apply edit for stretch operations - limit to simple functions
  */
  if(operation==tcStretchStart && ((tc)left)->operation!=tcSimple){
    /* free left and right ? */
    sprintf(msg,"NB000E The start stretch operator '<' requires a simple function on the left.");
    return(NULL);
    }  
  if(operation==tcStretchStop && ((tc)right)->operation!=tcSimple){
    /* free left and right ? */
    sprintf(msg,"NB000E The stop stretch operator '>' requires a simple function on the right.");
    return(NULL);
    }  
  /* build a tcDef structure */
  //if((tcdef=(tc)malloc(sizeof(struct tcDef)))==NULL) //2012-01-26 dtl: handled out of memory
  //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
  tcdef=(tc)nbAlloc(sizeof(struct tcDef));
  tcdef->operation=operation;
  tcdef->left=left;
  tcdef->right=right;
  *source=cursor;   /* return the cursor */  
  return(tcdef);    
  }
  
/****************************************************************************************
*  Time Condition Queue Routines
*  o These routines must implement the functionality of the nbsched.h routines.
*  o The COND structure for time conditions is changing in NodeBrain, and both will be
*    supported during a test period.
*       old  ->  cond.op=opTime, cond.left=NULL,    cond.right=SCHED
*       new  ->  cond.op=opTime, cond.left=tcQueue, cond.right=NULL
****************************************************************************************/

/*
*  Create a new time condition queue structure
*/
tcq tcQueueNew(tc tcdef,long begin,long end){
  tcq queue;
  
  //if((queue=(tcq)malloc(sizeof(struct tcQueue)))==NULL) //2012-01-26 dtl: handled out of memory
  //  {outMsg(0,'E',"malloc error: out of memory");exit(NB_EXITCODE_FAIL);} //dtl:added
  queue=(tcq)nbAlloc(sizeof(struct tcQueue));
  queue->tcdef=tcdef;
  /* we don't cast a stand-alone time procedure */
  /* instead we let schedNext() call nbRuleStep() */
  if(tcdef->operation==tcPlan){
    queue->set=NULL;
    ((NB_Rule *)tcdef->right)->time=nb_ClockTime; /* set start time */
    }
  else queue->set=tcCast(begin,end,tcdef);
  return(queue);
  } 

/*
*  Get time of next true state
*    o Returns maximum time under implementation limits when no true state is found.
*    o A casting domain is used to limit the number of intervals generated in advance.
*      If no interval is found in the specified domain, the domain is expanded until
*      an interval is found or the implementation limit is reached.  The domain size
*      gets progressively larger on each recast.
*    o We always expand the domain until the first "usable" interval is entirely
*      contained within the domain.  This is necessary to make sure that normalization
*      has fully extended the interval.  
*/  
long tcQueueTrue(tcq queue,long begin,long end){
  bfi interval;
  
  if(trace) outMsg(0,'T',"tcQueueTrue: called begin=%d,  end=%d.",begin,end);
  for(interval=queue->set->next;interval!=queue->set && interval->end<=begin;interval=interval->next){
    interval=bfiRemove(interval);  /* should we really be skipping intervals? */
    }
  while(queue->set->start!=maxtime && (interval==queue->set || interval->end>=queue->set->start)){  /* empty or unreliable set, get more intervals */
	  if(interval->end>end) end=interval->end; /* expand to enclose the interval */
    if(trace) outMsg(0,'T',"tcQueueTrue: casting new intervals");
    queue->set=bfiDispose(queue->set);
    queue->set=tcCast(begin,end,queue->tcdef);
    end+=end-begin;    /* prepare for next try if it becomes necessary */
    if(end<=begin) end=maxtime;
    interval=queue->set->next;
    }
  if(trace) outMsg(0,'T',"tcQueueTrue: return start=%d, stop=%d.",interval->start,interval->end); 
  return(interval->start);
  }
  
/*
*  Get time of next false state
*/  
long tcQueueFalse(tcq queue){
  return(queue->set->next->end);
  } 

/**********************************************************************
* Public Methods
**********************************************************************/
void nbTimePrintCalendar(NB_Calendar *calendar){
  outPut("%s",calendar->text->value);
  }
void nbTimeDestroyCalendar(NB_Calendar *calendar){
  /* free tcdef here */
  dropObject(calendar->text);
  //free(calendar); // 2012-10-12 eat - witch to nbFree
  nbFree(calendar,sizeof(NB_Calendar)); 
  }

void nbTimeInit(NB_Stem *stem){
  nb_TimeCalendarType=newType(stem,"calendar",NULL,0,nbTimePrintCalendar,nbTimeDestroyCalendar);
  nb_TimeCalendarContext=nbTermNew(NULL,"calendar",nbNodeNew());
  }

NB_Term *nbTimeLocateCalendar(char *ident){
  NB_Term *term;
  if((term=nbTermFind(nb_TimeCalendarContext,ident))==NULL) return(NULL);
  return(term);
  }

/*
*  Declare time expression
*/
NB_Term *nbTimeDeclareCalendar(nbCELL context,char *ident,char **source,char *msg){
  tc tcDef;
  NB_Term *term;
  NB_Calendar *calendar;
  struct STRING *text;
  char *cursor=*source,*string,delim;
  if(nbTermFind(nb_TimeCalendarContext,ident)!=NULL){
    snprintf(msg,(size_t)NB_MSGSIZE,"NB000E Calendar \"%s\" already declared.",ident); //dtl used snprintf
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  string=cursor;
  while(*cursor!=' ' && *cursor!=';' && *cursor!=0) cursor++;
  delim=*cursor;
  *cursor=0;
  text=grabObject(useString(string));
  tcDef=tcParse(context,&string,msg);
  *cursor=delim;
  if(tcDef==NULL){
    dropObject(text);
    return(NULL);
    }
  calendar=grabObject(newObject(nb_TimeCalendarType,(void **)&nb_TimeCalendarFree,sizeof(NB_Calendar)));
  calendar->tcdef=tcDef;
  calendar->text=text;
  term=nbTermNew(nb_TimeCalendarContext,ident,calendar);
  return(term);
  } 
  
