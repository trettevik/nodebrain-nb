/*
* Copyright (C) 1998-2013 The Boeing Company
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
* File:     nbclock.c 
*
* Title:    Internal Clock Routines
*
* Function:
*
*   This file provides routines that manage timers for alerting objects.  This
*   mechanism is fundamental to the implementation of time conditions, but may
*   be used by any object with an alert method.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void  nbClockInit();
*   void  nbClockSetTimer(time_t time,NB_Cell *object);
*   int   nbClockAlert();
*
*   tm   *nbClockGetTm(int zone,time_t time);
*   char *nbClockToBuffer(char *buffer);
*   char *nbClockToString(time_t utc,char *string);
*   void  nbClockShowTimers();
*
* Description
*
*   Timers are placed in a timer queue, currently implemented as a simple
*   linked list ordered by expiration time.  Each timer is associated with
*   an object.  When a timer expires, the object's alert method is called.
*   The select() function is used to wait for the next timer and input 
*   from listener files concurrently.
*   
*   nbClockInit()
*
*            Initializes the timer queue and other clock variables.
*
*   nbClockSetTimer(time_t time,NB_Cell *object)
*
*            Sets a timer for the specified object.  Only one timer may
*            be set for a given object.  A timer is cancelled by
*            specifying a time of zero.
*
*   nbClockAlert()
*
*            Updates clock time and alerts all objects whose timers have
*            expired.  Returns the number of seconds remaining until the
*            next timer expires.
* 
*   nbClockGetTm(int clock,time_t time)
*
*            Get time in broken down form.  The clock option determines
*            if localtime() or gmtime() is used.
*
*               NB_CLOCK_GMT     - gmtime()
*               NB_CLOCK_LOCAL   - localtime()
*               nb_clockClock    - default setting
*         
*   nbClockToBuffer(char *buffer)
*
*            Converts clock time to a 21 or 12 character null terminated date
*            and time stamp. Returns the address of the terminating null
*            character. This is designed for writing time stamps to a buffer.
*            The format is controlled by nb_clockFormat.
*
*               "yyyy/mm/dd hh:mm:ss "   Local and GMT format
*               "ssssssssss "             UTC format 
*
*   nbClockToString(time_t utc,char *string)
*
*            Converts specified time to a 21 or 12 character null terminated
*            date and time stamp.  Returns the address of the updated string.
*
*   nbClockShowTimers()
*
*            Prints a list of active timers.  Used by the SHOW command.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 1999-04-07 Ed Trettevik (original prototype version)
* 2003-11-02 eat 0.5.5  Renamed (was nbevent.c), reorganized, and fixed bugs.
*            1) Dropped obsolete change entries. 
*            2) Functions are now nbClockXxxxx().
*            3) Global variables are nb_clockXxxxx and nb_timerXxxxx.
*            4) Structures NB_TIMER, type NB_Timer.
*            5) nbClockAlert() calls nbCellReact() for each 1-second batch
*               of alerts to objects.  This ensures that reactions will take
*               place in the expected order by time.  Previously, multiple
*               seconds were batched when the queue was behind, typically in
*               interactive mode.  That enabled some out of order problems.
*               the expected order by time.
*            6) Removed logic watching for system clock changes.  It was not
*               causing more problems than it was solving.  If the system
*               clock is set forward at a time when many timers are set, a
*               bunch of timers will expire in a short period.  This may or
*               may not be a problem.  It is up to the objects alerted to
*               avoid doing crazy things like spawning hundreds of child
*               processes.
* 2008-05-24 eat 0.7.0  Included nbClockSetTimerInterval() function
* 2008-11-11 eat 0.7.3  Change failure exit code to NB_EXITCODE_FAIL
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2011-02-08 eat 0.8.5  Modified nbClockSetTimer to reset object timers
*            Previously it considered an attempt to reset a timer a logic error
*            and required a timer to be cleared before setting a new one.
* 2012-01-09 dtl 0.8.6  Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2013-01-13 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nbi.h> 

time_t nb_ClockTime;     /* current clock time */
time_t nb_ClockLocalOffset;  // local offset in seconds
time_t nb_clockOffset=0; /* offset applied before breakdown to tm structure */  
int    nb_clockClock=NB_CLOCK_LOCAL;  /* break down function: 0 - gmtime(), 1 - localtime() */
int    nb_clockFormat=1; /* format for displaying times */ 
                           /* 0 - UTC "ssssssssss ", 1 - "yyyy/mm/dd hh:mm:ss " */

NB_Timer *nb_timerQueue; /* timer queue */
NB_Timer *nb_timerFree;  /* free timers */
  
/*
*  Initialize clock timer structures
*/ 
void nbClockInit(NB_Stem *stem){
  time(&nb_ClockTime);
  nb_ClockLocalOffset=mktime(localtime(&nb_ClockTime))-mktime(gmtime(&nb_ClockTime));
  nb_timerQueue=NULL;
  nb_timerFree=NULL;
  }
 
/*
*  Set a timer to alert an object.
*/
void nbClockSetTimer(time_t etime,NB_Cell *object){
  NB_Timer **timerP,*newTimer;

  /* This can be improved by using a more complex structure.  A simple list seems
  *  to work, but one can imagine problems when the queue gets long.
  */
  for(timerP=&nb_timerQueue;*timerP!=NULL && object!=(NB_Cell *)(*timerP)->object;timerP=&((*timerP)->next));
  if(*timerP!=NULL){
    newTimer=*timerP;             // cancel existing timer
    *timerP=(*timerP)->next;
    newTimer->next=nb_timerFree;
    nb_timerFree=newTimer;
    }
  if(etime==0) return;
  if((newTimer=nb_timerFree)==NULL) newTimer=(NB_Timer *)nbAlloc(sizeof(NB_Timer));
  else nb_timerFree=newTimer->next;
  newTimer->time=etime;
  newTimer->object=(NB_Object *)object;  
  // 2008-05-24 eat - changed comparison to >= so events with same time fire in the order they were scheduled
  for(timerP=&nb_timerQueue;*timerP!=NULL && etime>=(*timerP)->time;timerP=&((*timerP)->next));
  newTimer->next=*timerP;
  *timerP=newTimer;
  }

void nbClockSetTimerInterval(int seconds,NB_Cell *object){
  time_t at;
  time(&at);
  at+=seconds;
  nbClockSetTimer(at,object); 
  }
  
/*
*  Alert all objects with expired timers
*
*    All objects whose timers expire in the same second are alerted
*    in a single cycle. This is similar to multiple assertions in
*    a single assert command. Individual objects may call nbCellReact
*    after publishing changes if immediate reaction is necessary. Here
*    we call nbCellReact at the end of each 1-second cycle to respond
*    to outstanding published changes.
*
*/
int nb_ClockAlerting=0;
int nbClockAlert(void){
  static long maxNap=NB_MAXNAP;
  int nap;
  NB_Timer *timer;
  NB_Object *object;
  int react=0;
  time_t cycleTime;;    /* time we found something to do */

  if(nb_ClockAlerting){
    outMsg(0,'L',"nbClockAlert() called while alerting.");
    exit(NB_EXITCODE_FAIL);
    }
  nb_ClockAlerting=1;
  //outMsg(0,'T',"nbClockAlert() called ...");
  
  time(&nb_ClockTime);
  while(nb_timerQueue!=NULL && nb_timerQueue->time<=nb_ClockTime){
    react=0;
    cycleTime=nb_timerQueue->time;  /* Process 1 second cycle */
    while(nb_timerQueue!=NULL && nb_timerQueue->time==cycleTime){
      timer=nb_timerQueue;
      nb_timerQueue=timer->next;
      timer->next=nb_timerFree;
      nb_timerFree=timer;
      if(timer->time!=0){           /* if timer not cancelled, alert the object */
        object=timer->object;
        (object->type->alarm)(object);
        react=1;
        }
      }
    time(&nb_ClockTime);
    if(react) nbRuleReact();
    }
  outFlush();
  if(nb_timerQueue==NULL || (nap=nb_timerQueue->time-nb_ClockTime)>maxNap) nap=maxNap;
  //outMsg(0,'T',"nbClockAlert() returning wait=%d seconds",nap);
  nb_ClockAlerting=0;
  return(nap);
  }

/*
*  Convert UTC to broken down time (struct tm *)
*
*     clock:  0 - default setting (see nb_clockClock), -1 - Local, 1 - GMT
*/
struct tm *nbClockGetTm(int clock,time_t utc){
  struct tm *clockTm;  /* time in broken down  form */
  /*  int tm_sec;      seconds after the minute - [0,61] */
  /*  int tm_min;      minutes after the hour - [0,59] */
  /*  int tm_hour;     hours - [0,23] */
  /*  int tm_mday;     day of month - [1,31] */
  /*  int tm_mon;      month of year - [0,11] */
  /*  int tm_year;     years since 1900 */
  /*  int tm_wday;     days since Sunday - [0,6] */
  /*  int tm_yday;     days since January 1 - [0,365] */
  /*  int tm_isdst;    daylight savings time flag */

  utc+=nb_clockOffset;
  if(clock==NB_CLOCK_GMT) clockTm=gmtime(&utc);
  else clockTm=localtime(&utc);
  return(clockTm);  
  }

/*
*  Convert a broken down GMT time into UTC
*    This is intended as a portable alternative to the GNU timegm function.
*/
time_t nbClockTimeGm(struct tm *tm){
  return(mktime(tm)-nb_ClockLocalOffset);
  }

/*
*  Convert clock time to character string and return address of null delimiter.
*/
char *nbClockToBuffer(char *buffer){
  struct tm *printTm;     /* time in structured form */

  time(&nb_ClockTime);
  if(nb_clockFormat==0){
    sprintf(buffer,"%.10d ",(int)nb_ClockTime);
    return(buffer+11);
    }
  printTm=nbClockGetTm(nb_clockClock,nb_ClockTime);
  sprintf(buffer,"%.4d/%.2d/%.2d %.2d:%.2d:%.2d ",
    printTm->tm_year+1900,printTm->tm_mon+1,printTm->tm_mday,
    printTm->tm_hour,printTm->tm_min,printTm->tm_sec);
  return(buffer+20);  
  }
      
/*
*  Convert UTC time to character string (nb_clockFormat determines format)
*/    
#define CTIME_SIZE 21    //buffer has at least sizeof 21 
char *nbClockToString(time_t utc,char *buffer){
  struct tm *printTm;

  if(nb_clockFormat==0){
    sprintf(buffer,"%.10d ",(int)utc);
    return(buffer);
    }
  printTm=nbClockGetTm(nb_clockClock,utc);
  snprintf(buffer,(size_t)CTIME_SIZE,"%.4d/%.2d/%.2d %.2d:%.2d:%.2d ",  //2012-01-09 dtl use snprintf
    printTm->tm_year+1900,printTm->tm_mon+1,printTm->tm_mday,
    printTm->tm_hour,printTm->tm_min,printTm->tm_sec); 
  return(buffer);
  }

/*
*  Display process cpu time
*/

void nbClockShowProcess(char *cursor){
  static clock_t start=0;
  clock_t end;
  double total,elapsed;

  end=clock();
  elapsed = ((double) (end - start)) / CLOCKS_PER_SEC;
  total = ((double) end) / CLOCKS_PER_SEC;
  outMsg(0,'I',"process total=%f  elapsed=%f  %s",total,elapsed,cursor); 
  start=end;
  }

// display a few subscribers if object is a cell
// bubble up to rules and nodes---we don't need to see every cell along the way
void nbClockShowSub(NB_Cell *cell,int *count){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Object *object;

  if(cell->object.value!=(NB_Object *)cell){ 
    NB_TREE_ITERATE(treeIterator,treeNode,cell->sub){
      object=(NB_Object *)treeNode->key;
      if(object->type==condTypeOnRule || (object->type->attributes&TYPE_RULE) || object->type==nb_NodeType){
        outPut("                      ");
        printObjectItem(object); 
        outPut("\n");
        (*count)--;
        }
      if(*count) nbClockShowSub((NB_Cell *)object,count);
      NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
      }
    }
  }

/*
*  Display all timers
*
*    Supports the "show -clock [{local|gmt|utc}]" command.
*    Cursor parameter points to option string---may be null string;
*/
void nbClockShowTimers(char *cursor){
  NB_Timer  *timer;
  char ctime[30],symid,ident[256];
  int nb_clockFormatSave=nb_clockFormat;
  int nb_clockClockSave=nb_clockClock;
  char *cursave=cursor;
  int subscribers;

  symid=nbParseSymbol(ident,sizeof(ident),&cursor);
  if(symid=='t'){
    if(strcmp(ident,"utc")==0)       {nb_clockFormat=0;}
    else if(strcmp(ident,"local")==0){nb_clockFormat=1;nb_clockClock=NB_CLOCK_LOCAL;}
    else if(strcmp(ident,"gmt")==0)  {nb_clockFormat=1;nb_clockClock=NB_CLOCK_GMT;}
    else{
      outMsg(0,'E',"Option \"%s\" not recognized - {local|gmt|utc} expected.",ident);
      return;
      }
    while(*cursor==' ') cursor++;
    cursave=cursor;
    symid=nbParseSymbol(ident,sizeof(ident),&cursor);
    if(symid!=';'){
      outMsg(0,'E',"Unexpected option at \"%s\".",cursave);
      nb_clockFormat=nb_clockFormatSave;
      nb_clockClock=nb_clockClockSave;
      return;
      }
    }
  else if(symid!=';'){
    outMsg(0,'E',"Expecting option {local|gmt|utc} at \"%s\".",cursave);
    nb_clockFormat=nb_clockFormatSave;
    nb_clockClock=nb_clockClockSave;
    return;
    }
  outPut("~ %sClock\n",nbClockToString(nb_ClockTime,ctime));
  for(timer=nb_timerQueue;timer!=NULL;timer=timer->next){
    outPut("~ %s",nbClockToString(timer->time,ctime));
    printObjectItem(timer->object);
    outPut("\n");
    subscribers=5; // limit the number of rules displayed - could be a parameter
    nbClockShowSub((NB_Cell *)timer->object,&subscribers);
    }
  nb_clockFormat=nb_clockFormatSave;
  nb_clockClock=nb_clockClockSave;
  }
