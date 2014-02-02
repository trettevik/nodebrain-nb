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
* File:     nb_cache.c 
*
* Title:    Cache Module (prototype)
*
* Function:
*
*   This module provides the "cache" skill.  It manages a cache table that is
*   a tree structure with nodes that contain pointers to cells and associated
*   counters and timers.  The counters are managed as table rows are added and
*   deleted.  This type of node is handy for event correlation involving
*   repetition and variation of sets of table attributes.
*
* Synopsis:
*
*   define <term> node cache:<spec>
*
*   <spec>       ::= ([[[!]~(<n><timeUnit>)][<thresholds>]:]<attrList>)
*   <thresholds> ::= [{<nList>}]["["<nList>"]"][(<nList>)]
*   <nList>      ::= n [,n [,n] ]
*   <attrList>   ::= attrSpec [, attrSpec ] ...
*   <attrSpec>   ::= <attrName>[<thresholds>]
*   
*
* Description
*
*   This module is a bit complex and is covered in the "NodeBrain Module
*   Reference".  It is probably best not to say too much here.  Since this
*   code has been converted to a skill module, the interaction with the  
*   interpreter is well defined.  Ok, well that will be true when this 
*   module is fully converted to using the nbapi.h header instead of the
*   nb.h header.  The important thing now is that there are not calls 
*   into this code except via the well defined skill methods.
*    
*=============================================================================
* Enhancements:
*
*   o If we insert fewer attributes than a cache supports, complete the row
*     with placeholder entries.
*   o Use a hash instead of a single list if the lists get long.
*   o Double linked lists would clean the logic up but use more space.  The
*     deletions could be done when the counters go to zero.  As is, the entries
*     towards the end of the list may not get deleted for a long time.
*   o Interval could be replaced with a schedule (probably not much use for that)
*   o We should schedule an alert when thresholds reset.  We have to provide
*     for multiple resets within a given alarm.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/05/30 Ed Trettevik (original prototype version)
* 2001/07/05 eat - version 0.2.8
*             1) Functions and variables have been renamed just for the fun
*                of it, to be more like the object method names used in
*                other headers.
*             2) Use of the term event has been replaced with object and entry.
*             3) Counters are now associated with objects of any type, not
*                just STRING.
*             4) cacheReset and cacheEmpty are new functions.
*             5) An interval of zero now means counters never decrement
*                automatically.
*             6) If the first threshold is zero, cacheInc returns the counter
*                without worrying about thresholds.
* 2001/12/28 eat - version 0.2.9d
*             1) Treat [0] threshold as reset threshold, call [1] threshold
*                the first threshold.
*             2) Changed the return value from cacheInc
*                  positive count if no thresholds or count below first threshold
*                    and the first threshold is active.
*                  negative count if we hit a threshold.
*                  zero if first threshold not active and not on a threshold.
* 2002/02/20 eat - version 0.3.0
*             1) Enhanced cache structures and functions to support multiple
*                attributes (dimensions) with user assigned names.
*
*                  define X context cache(~(4h)(h,.){r,.}[k,.]: \
*                    A(h,.)[k,.],B(h,.)[k,.],C(h,.));
*
*                    attribute(hit_thresholds){row_thresholds}[value_thresholds]
*                    attribute(^t0,t1,t2,t3){^t0,t1,t2,t3}[^t0,t1,t2,t3]
*
*                  assert ("value","value",...),attribute=value,...
*
*                The assert statement assigns the positional values to the
*                corresponding user defined name.  The assert statement examples
*                below are similar.
*
*                  define x context cache(~(4h):source[2],type(100,200));
*
*                  x assert ("192.168.1.1","CodeRed");
*
*                  x assert source="192.168.001.001",type="CodeRed";
*
*                If there are N attributes, there are N*3+1 counters and
*                states revealed to the context rules.
*
*                  _hits,_rows,_kids,A._hits,A._rows,A._kids,B._hits,...
*
*                  _hitState,_rowState,_kidState,A._hitState,A._rowState,...
*                    
*                A state variable has a value of "unknown", 0, "minor",
*                "major", and "critical" by
*                default, depending on the value of the counter relative to
*                the threshold.  The "unknown" value is set if the counter
*                has not reached a new threshold, otherwise it indicates the
*                threshold just reached.  A value of 0 is set if "alert" is
*                used instead of "assert", and the first threshold has not
*                been reached.
*
*                  define r1 if(A._hitState=0):... [alert and threshold not reached] 
*                  define r2 if(not B._kidState=0):... [new threshold reached] 
*
* 2002/02/26 eat - version 0.3.0
*             1) Added support for 0.2.9 compatibility.  If the following syntax
*                is found by nodebrain.c it will automatically call the nbcache0.h
*                routines newCache0() and cache0Inc() instead of newCache and
*                cacheAssert;
*
*                define x context  ~8h (0,10,20,50);
*
*                x cache "value",attribute=value,....
*
* 2002/08/20 eat - version 0.4.0
*             1) Included an alertCache function to begin prototyping a
*                new cache feature that will alert a context when cache rows
*                expire.  This will be used to delay event notification until
*                possible correlated events "fail" to show up in some time
*                interval.
* 2002/08/21 eat - version 0.4.0
*             1) Included support for an option to schedule cache alerts
*                when rows expire.
*
*                  define <term> context cache(!~(8h),a,b,c);
*
* 2002/08/22 eat - version 0.4.0
*             1) Renamed cacheInsert to cacheAssert and included a syntax for
*                removing rows before they expire.
*
*                  assert !("hi","there","buddy");
*
* 2002/08/25 eat - version 0.4.0
*             1) Modified cacheAssert() to support terms in addition to string
*                literals.  This required the addition of a context parameter
*                for term resolution.
*
*                  assert (system,file);
*
*             2) Split into nbcache.h and nbcache.c
*             3) Implemented cache conditions so we can retire the cache0.h
*                routines.
*
*                  <context>{<object_list>}
*
*                Unknown - if the object list is unresolved.
*                True    - if object list is resolved and a matching row is found.
*                False   - if object list is resolved and no matching row found.
*
* 2002/09/08 eat 0.4.1  Support numbers and functions as list members.
* 2003/03/15 eat 0.5.1  Modified for make file
* 2003/07/20 eat 0.5.4  Included non-counting scheduled cache option
*              
*                define c context cache(~(10m):a,b);
*
*                In this case, we only need one timer element to schedule the
*                expiration of an entry.  Instead of one timer element to 
*                decrement for each assertion, we only need a timer element
*                for the last assertion for each row.  The lack of thresholds
*                is the key to not counting.
*
* 2003/07/20 eat 0.5.4  Using consistent scheduled expiration with intervals.
*
*                Previously we only scheduled alerts to the cache when context
*                alerts where requested via the "!" option in front of the
*                interval. We left it up to the assertion routine to check for
*                expired rows.  Now we always schedule the expiration to get
*                the expected reaction in cache reference conditions.
*
*                  define r1 on(a=1 and c("fred","smith")):...
*
* 2004/09/24 eat 0.6.1  Cancel timer when no more scheduled events
* 2004/09/24 eat 0.6.1  Fixed bug that allowed change to go unpublished
* 2005/04/09 eat 0.6.2  converted to fully conform to skill module API
* 2005/04/30 eat 0.6.2  refined assert/alert methods a bit
*
*                We now handle multiple cache assertions within a single
*                ASSERT/ALERT statement.  It was not so important to enable
*                multiple assertions for a single cache, but that is a side
*                effect of our implementation.  Here is an example showing
*                what we wanted to clean up.
*                
*                  define c1 node cache:(a,b(3));
*                  define c2 node cache:(x(5));
*
*                  c1. assert ("abc","def"),a=1,b=2,c2("hi"),d=4;
*
*                Prior to this fix, the c2("hi") assertion would have split
*                the assertion when the threshod of 5 was reached.  This would
*                mean rules could react to changes in a and b before the
*                assertion of d=4 was complete.  That violated our claim that
*                our reaction to an ASSERT statement is started only after all
*                of the assertions are complete. With this fix, that claim is
*                true again.
*
* 2005/04/30 eat 0.6.2  Did not eliminate all dependence on nb.h
*
*                Back on 4/9 I claimed to have made this module fully conform
*                to the Skill Module API.  This is true with respect 
*                the skill methods, but this module still references NodeBrain
*                internals exposed by nb.h.  I had hoped to finish converting
*                this module to enable the use of nbapi.h instead of nb.h. That
*                will have to wait for a future release.  For now, it must be
*                compiled with and included in nb.
*
* 2005/05/13 eat 0.6.3  warning messages for assertion of too many arguments
* 2005/05/13 eat 0.6.3  warning messages for assertion of too few arguments.
* 2005/05/14 eat 0.6.3  cacheBind() modified to accept moduleHandle
* 2008/02/10 eat 0.6.9  converted lists to trees to improve performace for large tables
* 2008/02/13 eat 0.6.9  fixed defect in handling of null argument list: assert ?node();
* 2008/10/01 eat 0.7.2  improved performance when removing timer elements
*         
*                The timer set is now a double linked list and the last entry of
*                a row points to the timer entry if we are not counting nodes.
*                In that case, there is only one timer entry per row. 
*
* 2008/10/03 eat 0.7.2  Included release condition argument
*
*                A release condition is used to empty a cache each time the
*                condition transitions to true.  It is really just a more
*                convenient way to express a rule to empty the cache.  The
*                following cache is emptied every 8 hours.
*
*                   define myCache node cache(~(8h)):(a,b,c);
*
*                Previously we would have implemented the same functionality
*                as follows
*
*                   define myCache node cache:(a,b,c);
*                   myCache. define release on(~(8h)) ?myCache();
*
* 2008/10/23 eat 0.7.3  Now using standard API header nb.h
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2012-12-27 eat 0.8.13 Checker updates
*=============================================================================
*/
#include "config.h"
#include <nb/nb.h>     

# define CACHE_THRESHOLD_INDEX_LIMIT  4

extern struct CACHE_TIMER   *cacheTimerFree;
extern struct CACHE_NODE    *cacheEntryFree;
extern struct CACHE_ATTR    *cacheAttrFree;
extern struct CACHE         *cacheFree;

struct CACHE_TIMER{
  struct CACHE_TIMER *prior;
  struct CACHE_TIMER *next;
  struct CACHE_NODE  *entry;
  time_t             time;
  };

struct CACHE_NODE{                 /* attribute value counter entry */
  // next four fields must conform to NB_TreeNode structure
  struct CACHE_NODE *left;         // left entry in this tree */
  struct CACHE_NODE *right;        // right entry in this tree */
  signed int        balance;       // AVL balance code (-1 left tall, 0 - balanced, +1 right tall)
  nbCELL            object;        // object pointer - NULL on free list 
  //
  struct CACHE_NODE *root;         /* root entry of this list */
  struct CACHE_NODE *entry;        /* subordinate nodes */
  unsigned int      hits;          /* times asserted in the cache interval */
  unsigned int      rows;          /* rows retained in cache interval */
  unsigned int      kids;          /* subordinate entries */
  unsigned char     hitIndex;      /* index to active hit threshold */
  unsigned char     rowIndex;      /* index to active row threshold */
  unsigned char     kidIndex;      /* index to active kid threshold */
  unsigned char     flags;         // flag bits
  };

#define CACHE_NODE_FLAG_LASTCOL 1  // node is in last column

struct CACHE_ATTR{                  /* Attribute definition */
  struct CACHE_ATTR  *next;         /* next attribute */
  struct CACHE_ATTR  *prev;         /* prev attribute */
  nbCELL             term;         /* attribute term */
  nbCELL             hitsTerm;     /* attribute._hits term */
  nbCELL             rowsTerm;     /* attribute._rows term */
  nbCELL             kidsTerm;     /* attribute._kids term */
  nbCELL             hitState;      /* attribute._hitState term */
  nbCELL             rowState;      /* attribute._rowState term */
  nbCELL             kidState;      /* attribute._kidState term */
  unsigned int       hitThresh[CACHE_THRESHOLD_INDEX_LIMIT+1];/* hit thresholds */
  unsigned int       rowThresh[CACHE_THRESHOLD_INDEX_LIMIT+1];/* row thresholds */
  unsigned int       kidThresh[CACHE_THRESHOLD_INDEX_LIMIT+1];/* kid thresholds */
  };

typedef struct CACHE{
  struct CACHE       *next;        /* next cache on free list */
  nbCELL             action;       /* action causing an alert */
  nbCELL             context;      /* context term owning this cache */
  nbCELL             node;         /* node owning this cache */
  nbCELL             releaseCell;    // Cell (perhaps schedule) that triggers reset
  nbCELL             releaseSynapse; // synapse - used to respond to reset cell 
  struct CACHE_ATTR  *attr;        /* attribute list */
  struct CACHE_ATTR  *lastattr;    /* last attribute */
  struct CACHE_NODE  *entry;       /* object tree */
  struct CACHE_TIMER *timer;       // root entry for set of timers
  nbCELL             stateVal[CACHE_THRESHOLD_INDEX_LIMIT];
  int                interval;     /* interval of time to retain cached entries */
  unsigned char      options;      // option bits - see CACHE_OPTION_* below
  unsigned char      state;        // state bits - see CACHE_STATE_* below
  char               trace;        // set for debug
  nbSET              assertion;    /* assertion to schedule if necessary */
  nbCELL             expireCell;   // The string "expire"
  nbCELL             insertCell;   // The string "insert"
  nbCELL             addCell;      // The string "add"
  nbCELL             deleteCell;   // The string "delete"
  } NB_Cache;

#define CACHE_OPTION_COUNT  1      // Count hits
#define CACHE_OPTION_EXPIRE 2      // Row expiration alerts requested
#define CACHE_OPTION_EXIST  4      // Row existence alerts requested (insert and delete)

#define CACHE_STATE_PUBLISH 1      // Set when entries inserted or deleted
#define CACHE_STATE_ALERT   2      // Set when thresholds reached
#define CACHE_STATE_ALARM   4      // Set when cache alarm timer is set

// Cache skill memory   
struct CACHE_SKILL{
  nbCELL unknown;                                  // NodeBrain's unknown value
  nbCELL stateVal[CACHE_THRESHOLD_INDEX_LIMIT];    // Threshold state values
  };

/*====================================================*/

struct CACHE_TIMER   *cacheTimerFree=NULL;
struct CACHE_NODE    *cacheEntryFree=NULL;
struct CACHE_ATTR    *cacheAttrFree=NULL;
struct CACHE         *cacheFree=NULL;

int cacheParseThreshold();
struct CACHE_ATTR *newCacheAttr();
struct CACHE *newCache();
void printCacheRows();
void printCache();
struct CACHE_NODE *cacheFindRow();
int cacheGetCount();
void cacheNewTimerElement();
int cacheInsert();
void alertCache();
int cacheAssertParse();
void cacheFreeNode();
void cacheEmptyNode();
void cacheRemoveNode();
void cacheDecNode(nbCELL context,struct CACHE *cache,struct CACHE_NODE *entry,struct CACHE_ATTR *attr);
int cacheRemove();
void cacheEmpty(nbCELL context,struct CACHE *cache);
void freeCache();

/*
*  Parse threshold list
*/
int cacheParseThreshold(context,threshold,source)
  nbCELL context;
  unsigned int threshold[CACHE_THRESHOLD_INDEX_LIMIT];
  char **source; {

  char symid,token[256],stopchar;
  int i=1;
  
  if(**source=='(') stopchar=')';
  else if(**source=='{') stopchar='}';
  else if(**source=='[') stopchar=']';
  else{
    nbLogMsg(context,0,'L',"Expecting list starting with '(', '[', or '{'");
    return(-1);
    }
  (*source)++;
  if(**source=='^'){
    (*source)++;
    i=0;
    }
  else threshold[0]=0;
  symid=nbParseSymbol(token,sizeof(token),source);
  for(;i<=CACHE_THRESHOLD_INDEX_LIMIT && symid=='i';i++){
    threshold[i]=atoi(token);
    symid=nbParseSymbol(token,sizeof(token),source);
    if(symid==',') symid=nbParseSymbol(token,sizeof(token),source);
    }
  if(symid=='i'){
    nbLogMsg(context,0,'E',"A maximum of %d thresholds may be specified.",CACHE_THRESHOLD_INDEX_LIMIT);
    return(-1);
    }
  threshold[i]=0;                /* plug delimiter */
  if(**source!=stopchar){
    nbLogMsg(context,0,'E',"Expecting list delimiter '%c' at \"%s\"",stopchar,*source);
    return(-1);
    }
  (*source)++;    
  while(**source==' ') (*source)++;  /* skip over space */
  return(0);
  }

/*
*  Create a new cache attribute definition list
*  
*    (h,.){r,.}[k,.]           - special case for level zero
*    <term>(h,.){r,.}[k,.]
*
*/
struct CACHE_ATTR *newCacheAttr(nbCELL context,char **source,int level,struct CACHE_ATTR **backattr,int *threshflag){ 
  char symid,*cursor=*source,*cursave,ident[256];
  struct CACHE_ATTR *attr=NULL;
  int prefix=1;

  while(*cursor==' ') cursor++;
  
  if((attr=cacheAttrFree)!=NULL) cacheAttrFree=attr->next;
  else attr=nbAlloc(sizeof(struct CACHE_ATTR));
  *backattr=attr;         /* plug link to last attribute */
  attr->next=NULL;
  attr->prev=NULL;
  attr->term=NULL;
  attr->hitsTerm=NULL;
  attr->hitState=NULL;
  attr->rowsTerm=NULL;
  attr->rowState=NULL;
  attr->kidsTerm=NULL;
  attr->kidState=NULL;
  attr->hitThresh[0]=0;
  attr->hitThresh[1]=0;
  attr->rowThresh[0]=0;
  attr->rowThresh[1]=0;
  attr->kidThresh[0]=0;
  attr->kidThresh[1]=0;
  if(level==0){
    if((*cursor>='a' && *cursor<='z') || (*cursor>='A' && *cursor<='Z')) prefix=0;
    else if(strchr("({[:",*cursor)==NULL){
      nbLogMsg(context,0,'E',"Unexpected character at \"%s\"",cursor);
      attr->next=cacheAttrFree;
      cacheAttrFree=attr;
      return(NULL);
      }
    attr->term=context;
    }
  else{
    cursave=cursor;
    symid=nbParseSymbol(ident,sizeof(ident),&cursor);
    if(symid!='t'){
      nbLogMsg(context,0,'E',"Expecting attribute name at \"%s\"",cursave);
      }
    attr->term=nbTermCreate(context,ident,NB_CELL_UNKNOWN);
    }
  while(strchr("({[",*cursor)){
    switch(*cursor){
      case '(':
        *threshflag|=1;
        attr->hitsTerm=nbTermCreate((nbCELL)attr->term,"_hits",NB_CELL_UNKNOWN);
        attr->hitState=nbTermCreate((nbCELL)attr->term,"_hitState",NB_CELL_UNKNOWN);
        if(cacheParseThreshold(context,attr->hitThresh,&cursor)!=0) return(NULL);
        break;
      case '{':
        *threshflag|=2;
        attr->rowsTerm=nbTermCreate((nbCELL)attr->term,"_rows",NB_CELL_UNKNOWN);
        attr->rowState=nbTermCreate((nbCELL)attr->term,"_rowState",NB_CELL_UNKNOWN);
        if(cacheParseThreshold(context,attr->rowThresh,&cursor)!=0) return(NULL);
        break;
      case '[':
        *threshflag|=4;
        attr->kidsTerm=nbTermCreate((nbCELL)attr->term,"_kids",NB_CELL_UNKNOWN);
        attr->kidState=nbTermCreate((nbCELL)attr->term,"_kidState",NB_CELL_UNKNOWN);
        if(cacheParseThreshold(context,attr->kidThresh,&cursor)!=0) return(NULL);
        break;
      }
    }
  if((level>0 && *cursor==',') || prefix==0 || (level==0 && *cursor==':')){
    if(prefix) cursor++;
    attr->next=newCacheAttr(context,&cursor,level+1,backattr,threshflag);
    if(attr->next) attr->next->prev=attr;
    else{
      attr->next=cacheAttrFree;
      cacheAttrFree=attr;
      return(NULL);
      }
    }
  else attr->next=NULL;
  *source=cursor;
  return(attr);
  }

/*
*  Create a new cache
*
*     (a,b,c)
*     (~(4h):a,b,c)
*     (!~(4h):a,b,c)
*     (~(4h)(1000,2000):a,b,c)
*     (~(8h)(1000,2000){600,900,1000}[20]:source(200,250)[2],type(50,100))
*/
struct CACHE *newCache(nbCELL context,char *cursor){
  int  interval=0,threshflag=0;
  char intervalStr[32];
  char symid,token[256]; 
  struct CACHE *cache;
  struct CACHE_NODE *entry;
  struct CACHE_TIMER *timer;


  if((entry=cacheEntryFree)==NULL)
    entry=nbAlloc(sizeof(struct CACHE_NODE));
  else cacheEntryFree=entry->left;
  entry->left=NULL;
  entry->right=NULL;
  entry->root=NULL;
  entry->object=NULL;
  entry->hits=0;
  entry->rows=0;
  entry->kids=0;
  entry->hitIndex=1;
  entry->rowIndex=1;
  entry->kidIndex=1;
  entry->flags=0;
  entry->entry=NULL;

  if((timer=cacheTimerFree)==NULL) timer=nbAlloc(sizeof(struct CACHE_TIMER));
  else cacheTimerFree=timer->next;
  timer->prior=timer;
  timer->next=timer;
  timer->entry=NULL;
  timer->time=0;

  if((cache=cacheFree)==NULL) cache=nbAlloc(sizeof(struct CACHE));
  else cacheFree=cache->next;
  cache->context=context;
  cache->node=nbTermGetDefinition(context,context);
  //cache->action=NULL;
  cache->attr=NULL;
  cache->lastattr=NULL;
  cache->entry=entry;
  cache->timer=timer;  // only required under some conditions but always creating for now
  cache->interval=0;
  cache->options=0;
  cache->state=0;
  cache->assertion=NULL;
  cache->trace=0;
  cache->expireCell=nbCellCreateString(context,"expire");
  cache->insertCell=nbCellCreateString(context,"insert");
  cache->addCell=nbCellCreateString(context,"add");
  cache->deleteCell=nbCellCreateString(context,"delete");
  cache->action=nbTermCreate(context,"_action",cache->insertCell);

  /* parse definition string */
  if(*cursor=='?'){
    cache->options|=CACHE_OPTION_EXIST;  // alert on row add or delete
    cursor++;
    }
  if(*cursor!='('){
    nbLogMsg(context,0,'L',"Expecting left parenthesis at \"%s\"",cursor);
    return(NULL);
    }
  cursor++;
  while(*cursor==' ') cursor++;
  if(*cursor=='!'){  /* handle option for alert on row expiration */
    cache->options|=CACHE_OPTION_EXPIRE;
    cursor++;
    while(*cursor==' ') cursor++;
    }
  if(*cursor=='~'){  /* handle interval */
    cursor++;
    if(*cursor!='('){
      nbLogMsg(context,0,'E',"Expecting left parenthesis after tilda");
      return(NULL);
      }
    cursor++;
    symid=nbParseSymbol(token,sizeof(token),&cursor);
    if(symid!='i'){
      nbLogMsg(context,0,'E',"Expecting number to begin interval specification.");
      return(NULL);
      }
    interval=atoi(token);
    switch(*cursor){
      case 's': sprintf(intervalStr,"%d seconds",interval); break;
      case 'm': sprintf(intervalStr,"%d minutes",interval); interval*=60; break;
      case 'h': sprintf(intervalStr,"%d hours",interval);   interval*=60*60; break;
      case 'd': sprintf(intervalStr,"%d days",interval);    interval*=60*60*24; break;
      default:
        nbLogMsg(context,0,'E',"Expecting interval ending with 's', 'm', 'h', or 'd'.");
        return(NULL);
      }
    cursor++;
    if(*cursor!=')'){
      nbLogMsg(context,0,'E',"Expecting right parenthisis to close interval specification.");
      return(NULL);
      }
    cache->interval=interval;
    nbTermCreate(context,"_interval",nbCellCreateString(context,intervalStr));
    cursor++;
    while(*cursor==' ') cursor++;
    }
  cache->attr=newCacheAttr(context,&cursor,0,&(cache->lastattr),&threshflag);
  if(cache->attr==NULL){
    nbLogMsg(context,0,'E',"Cache attribute and threshold list not recognized.");
    nbFree(cache,sizeof(struct CACHE));
    return(NULL);
    }
  if(*cursor!=')'){
    nbLogMsg(context,0,'E',"Expecting right parenthesis at \"%s\"",cursor);
    /* include code to free the attribute list */
    return(NULL);
    }

  if(threshflag&1) cache->options|=CACHE_OPTION_COUNT;  // need to count when hit thresholds set

  cursor++;
  while(*cursor==' ') cursor++;
  if(*cursor!=0 && *cursor!=';'){
    nbLogMsg(context,0,'E',"Expecting ';' or end-of-line at: %s",cursor);
    // include call to free the attribute list
    return(NULL);
    }
  return(cache);
  }

struct CACHE_NODE *cacheFindRow(nbCELL context,struct CACHE *cache,nbSET argSet,struct CACHE_ATTR **attrP){
  struct CACHE_NODE *entry;
  //NB_Object *object;
  //nbCELL object;
  nbCELL argCell;

  if(cache->trace) nbLogMsg(cache->context,0,'T',"cacheFindRow: called");
  entry=cache->entry; /* start with root entry */
  argCell=nbListGetCellValue(context,&argSet);
  *attrP=cache->attr;  // start with first attribute
  while(argCell!=NULL){
    // replace this with inline macro after testing 
    entry=(struct CACHE_NODE *)nbTreeFind(argCell,(NB_TreeNode *)entry->entry);
    if(entry==NULL || entry->object!=argCell) return(NULL);
    argCell=nbListGetCellValue(context,&argSet);
    *attrP=(*attrP)->next;
    }
  if(cache->trace) nbLogMsg(cache->context,0,'T',"cacheFindRow() found an entry");
  return(entry);
  }

/*
*  Find an object in a cache and return the count
*          
*  The type code indicates which count to return.
*/
int cacheGetCount(nbCELL context,struct CACHE *cache,nbSET argSet,int type){
  struct CACHE_NODE *entry;
  struct CACHE_ATTR *attr;

  if(cache->trace) nbLogMsg(cache->context,0,'T',"cacheGetCount(): called");
  if((entry=cacheFindRow(context,cache,argSet,&attr))==NULL) return(0);
  if(cache->trace) nbLogMsg(cache->context,0,'T',"cacheGetCount(): found row");
  if(type==0) return(entry->hits);
  else if(type==1) return(entry->rows);
  else if(type==2) return(entry->kids);
  nbLogMsg(cache->context,0,'L',"cacheGetCount: counter type code %d not recognized.",type);
  return(0);
  }

/*
*  Create new timer element
*/
void cacheNewTimerElement(struct CACHE *cache,struct CACHE_NODE *entry){ 
  struct CACHE_TIMER *timer,*oldtimer;
  time_t now;

  if((timer=cacheTimerFree)==NULL) timer=nbAlloc(sizeof(struct CACHE_TIMER));
  else cacheTimerFree=timer->next;
  timer->prior=cache->timer->prior;
  timer->next=cache->timer;
  timer->prior->next=timer;
  timer->next->prior=timer;
  timer->entry=entry;
  time(&now);
  timer->time=now+cache->interval;
  if(cache->timer->next==timer && cache->timer->prior==timer){
    /* schedule cache alarm when adding first timer element */
    nbClockSetTimer(timer->time,cache->node);
    cache->state|=CACHE_STATE_ALARM;
    }
  if(cache->options^CACHE_OPTION_COUNT){ // manage single timer element per row when not counting
    if(entry->entry!=NULL){
      oldtimer=(struct CACHE_TIMER *)entry->entry;
      oldtimer->prior->next=oldtimer->next;
      oldtimer->next->prior=oldtimer->prior; 
      oldtimer->next=cacheTimerFree;
      oldtimer->entry=NULL;
      cacheTimerFree=oldtimer;
      }
    entry->entry=(struct CACHE_NODE *)timer;  // point node to timer element
    }
  }

/*
*  Insert new entry
*
*    This is a recursive function that operates at the node level.
*      
*  Return Code:
*
*    0 - entry already existed
*    1 - new entry added
*/
int cacheInsert(nbCELL context,struct CACHE_SKILL *skillHandle,struct CACHE *cache,struct CACHE_NODE *root,nbSET argSet,struct CACHE_ATTR *attr,int mode){
  NB_TreePath treePath;
  nbCELL object;
  //NB_Cell *pub;
  struct CACHE_NODE *entry;
  int  newrow=0;
  nbCELL argCell=nbListGetCellValue(context,&argSet);

  if(attr==NULL){
    nbLogMsg(context,0,'L',"cacheInsert: attr or object is null");
    return(-1);
    }

  /* increment root hits count */
  if(cache->options&CACHE_OPTION_COUNT){
    root->hits++;
    if(attr->hitsTerm!=NULL){
      /* assign value to attribute._hits */
      nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->hitsTerm,nbCellCreateReal(context,(double)root->hits));
      if(attr->hitState!=NULL){
        // compute new state and assign value to attribute._hitState
        if(attr->hitThresh[root->hitIndex]!=0 && root->hits>=attr->hitThresh[root->hitIndex]){
          cache->state|=CACHE_STATE_ALERT;
          nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->hitState,skillHandle->stateVal[root->hitIndex]);
          root->hitIndex++;
          }
        // in alert mode, set a normal state value until we hit the first threshold
        else if(mode==1 && root->hitIndex==1) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->hitState,skillHandle->stateVal[0]);
        else nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->hitState,(nbCELL)NB_CELL_UNKNOWN);
        }
      }
    }
  else{root->hits=1;}

  if(attr->next==NULL){    /* return if last attribute */
    argCell=nbListGetCellValue(context,&argSet);  // see if we have extra arguments
    if(argCell!=NULL){
      nbLogMsg(context,0,'W',"Extra assertion arguments ignored");
      }
    if(cache->interval) cacheNewTimerElement(cache,root);
    root->flags|=CACHE_NODE_FLAG_LASTCOL;
    if(root->rows==0){
      root->rows=1;
      return(1);
      }
    return(0); 
    }
  if(argSet==NULL){
    nbLogMsg(context,0,'W',"Placeholder used for unspecified assertion arguments");
    object=NB_CELL_PLACEHOLDER;
    }
  else if(argCell==NULL){
    nbLogMsg(context,0,'L',"cacheInsert: object is null");
    return(-1);
    }
  else object=argCell;  // assign value to attribute term in context

  nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->next->term,(nbCELL)object);
  entry=(struct CACHE_NODE *)nbTreeLocate(&treePath,object,(NB_TreeNode **)&root->entry);
  if(entry==NULL){
    /* create an entry here */
    if((entry=cacheEntryFree)==NULL) entry=nbAlloc(sizeof(struct CACHE_NODE));
    else cacheEntryFree=entry->left;
    entry->object=object;
    nbTreeInsert(&treePath,(NB_TreeNode *)entry);
    entry->root=root;
    entry->entry=NULL;
    entry->hits=0;
    entry->rows=0;
    entry->kids=0;
    entry->hitIndex=1;  /* zero index is for reset threshold */
    entry->rowIndex=1;  /* zero index is for reset threshold */
    entry->kidIndex=1;  /* zero index is for reset threshold */
    entry->flags=0;

    root->kids++;
    if(attr->kidState!=NULL){
      if(attr->kidThresh[root->kidIndex]!=0 && root->kids>=attr->kidThresh[root->kidIndex]){
        cache->state|=CACHE_STATE_ALERT;
        nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidState,skillHandle->stateVal[root->kidIndex]);
        root->kidIndex++;
        }
      else if(mode==1 && root->kidIndex==1) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidState,skillHandle->stateVal[0]);
      else nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidState,(nbCELL)NB_CELL_UNKNOWN);
      }
    cache->state|=CACHE_STATE_PUBLISH;  // set publish flag
    }
  else{
    nbCellDrop(context,object);  // drop the objects we don't add
    if(attr->kidState!=NULL){
      if(mode==1 && root->kidIndex==1) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidState,skillHandle->stateVal[0]);
      else nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidState,NB_CELL_UNKNOWN);
      }
    }

  if(attr->kidsTerm!=NULL) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->kidsTerm,nbCellCreateReal(context,(double)root->kids));

  /* handle next attribute */
  //if(list!=NULL) list=list->next;
  newrow=cacheInsert(context,skillHandle,cache,entry,argSet,attr->next,mode);
  if(newrow<0){
    // back out the node here
    return(newrow);
    }
  if(newrow){
    root->rows++;
    if(attr->rowState!=NULL){
      if(attr->rowThresh[root->rowIndex]!=0 && root->rows>=attr->rowThresh[root->rowIndex]){
        cache->state|=CACHE_STATE_ALERT;
        nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowState,skillHandle->stateVal[root->rowIndex]);
        root->rowIndex++;
        }
      // in alert mode, set a normal state value until we hit the first threshold
      else if(mode==1 && root->rowIndex==1) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowState,skillHandle->stateVal[0]);
      else nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowState,(nbCELL)NB_CELL_UNKNOWN);
      }
    newrow=1;
    }
  else if(attr->rowState!=NULL){
    // in alert mode, set a normal state value until we hit the first threshold
    if(mode==1 && root->rowIndex==1) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowState,skillHandle->stateVal[0]);
    else nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowState,(nbCELL)NB_CELL_UNKNOWN);
    }
  if(attr->rowsTerm!=NULL) nbAssertionAddTermValue(context,&cache->assertion,(nbCELL)attr->rowsTerm,nbCellCreateReal(context,(double)root->rows));

  return(newrow);
  }

/*
*  Cache reset alarm handler
*/

void cacheResetAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  NB_Cache *cache=(NB_Cache *)nodeHandle;
  nbCELL value=nbCellGetValue(context,cell);

  if(value!=NB_CELL_TRUE) return;  // only act when schedule toggles to true
  cacheEmpty(context,cache);
  }

/*
*  Cache alarm handler - decrement counters and remove expired rows
*
*    o We should actually use event time here, not system clock time
*      because we may want to do a simulation.
*    o This function is called by alertContext.
*    o The alertContext alert is scheduled when a row is added to an empty
*      cache, and rescheduled by alertCache if not emptied.
*    o This only applies to a cache with an expiration interval.
*/
void cacheAlarm(nbCELL context,void *skillHandle,NB_Cache *cache){
  struct CACHE_TIMER *timer,*timerRoot=cache->timer;
  struct CACHE_NODE *entry;
  struct CACHE_ATTR *attr;
  time_t now;

  cache->state&=0xff^CACHE_STATE_ALARM;
  time(&now);
  if(cache->options&CACHE_OPTION_EXPIRE){
    nbTermSetDefinition(context,cache->action,cache->expireCell);
    for(attr=cache->attr;attr!=NULL;attr=attr->next){
      if(attr->hitState!=NULL) nbTermSetDefinition(context,attr->hitState,NULL);
      if(attr->rowState!=NULL) nbTermSetDefinition(context,attr->rowState,NULL);
      if(attr->kidState!=NULL) nbTermSetDefinition(context,attr->kidState,NULL);
      }
    }
  timer=timerRoot->next;
  while(timer!=timerRoot && timer->time<=now){
    int expire=0;
    timerRoot->next=timer->next;
    timer->next->prior=timerRoot;
    timer->next=cacheTimerFree;
    cacheTimerFree=timer;
    if(cache->options&CACHE_OPTION_EXPIRE && timer->entry->hits<2){
      attr=cache->lastattr;
      for(entry=timer->entry;entry!=cache->entry;entry=entry->root){
        nbTermSetDefinition(context,attr->term,entry->object);
        attr=attr->prev;
        }
      expire=1;
      }
    cacheDecNode(context,cache,timer->entry,cache->lastattr);
    timer->entry=NULL;
    if(expire){
      nbRuleReact();
      nbNodeAlert(context,cache->context);
      }
    timer=timerRoot->next;
    }
  if(cache->options&CACHE_OPTION_EXPIRE) nbTermSetDefinition(context,cache->action,cache->insertCell);
  if(cache->state^CACHE_STATE_ALARM && cache->timer->next!=cache->timer){
    nbClockSetTimer(cache->timer->next->time,cache->node);
    cache->state|=CACHE_STATE_ALARM;
    }
  if(cache->state&CACHE_STATE_PUBLISH) nbCellPub(context,cache->context);
  }

/*
*  Free entry - this should be done in-line after validation
*/
void cacheFreeNode(nbCELL context,struct CACHE_NODE *entry){
/*
  outMsg(0,'T',"cacheFreeNode called");
  printObject(entry->object);
  nbLogPut(context,"\n");
*/

  nbCellDrop(context,entry->object);
  entry->object=NULL;
  entry->left=cacheEntryFree;   /* return to free list */
  entry->right=NULL;
  entry->root=NULL;
  entry->entry=NULL;
  entry->flags=0;
  cacheEntryFree=entry;
  }

/*
*  Empty a cache entry's object tree without worrying about counters
*/
void cacheEmptyNode(nbCELL context,struct CACHE_NODE *entry){
  struct CACHE_NODE *subEntry;
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;

  //nbLogMsg(context,0,'T',"cacheEmptyNode called");
  //printObject(entry->object);
  //nbLogPut(context,"\n");

  NB_TREE_ITERATE2(treeIterator,treeNode,(NB_TreeNode *)entry->entry){
    subEntry=(struct CACHE_NODE *)treeNode;
    if(subEntry->flags^CACHE_NODE_FLAG_LASTCOL) cacheEmptyNode(context,subEntry);
    NB_TREE_ITERATE_NEXT2(treeIterator,treeNode)
    cacheFreeNode(context,subEntry);
    }
  entry->entry=NULL;
  }

/*
*  Remove a cache entry worrying about counters up the root list       
*/
void cacheRemoveNode(nbCELL context,struct CACHE *cache,struct CACHE_NODE *entry,struct CACHE_ATTR *attr){
  NB_TreePath treePath;
  NB_TreeNode *treeNode;
  struct CACHE_NODE *root;
  int hits=entry->hits,rows=entry->rows;

  while(entry->root!=NULL){  /* until we get to the top */
    root=entry->root;
    treeNode=nbTreeLocate(&treePath,entry->object,(NB_TreeNode **)&root->entry);
    if(treeNode!=(NB_TreeNode *)entry){
      nbLogMsg(context,0,'L',"cache node not found in owning tree - aborting");
      exit(1);
      }
    nbTreeRemove(&treePath);
    if(entry->entry!=NULL && entry->flags^CACHE_NODE_FLAG_LASTCOL) cacheEmptyNode(context,entry);
    cacheFreeNode(context,entry);
    cache->state|=CACHE_STATE_PUBLISH;
    if(root->entry!=NULL || root->root==NULL){
      attr=attr->prev;
      root->kids--;
      if(root->kids<attr->kidThresh[0]) root->kidIndex=1;
      while(root!=NULL){
        if(cache->options&CACHE_OPTION_COUNT){ /* if we are counting */
          root->hits-=hits;
          if(root->hits<attr->hitThresh[0]) root->hitIndex=1;
          }
        root->rows-=rows;
        if(root->rows<attr->rowThresh[0]) root->rowIndex=1;
        root=root->root;
        attr=attr->prev;
        }
      return;
      }
    entry=root;       
    attr=attr->prev;
    }
  }

void cacheDecNode(nbCELL context,struct CACHE *cache,struct CACHE_NODE *entry,struct CACHE_ATTR *attr){
  if(entry->hits<2) cacheRemoveNode(context,cache,entry,attr);
  else if(cache->options&CACHE_OPTION_COUNT) while(entry!=NULL){
    entry->hits--;
    if(entry->hits<attr->hitThresh[0]) entry->hitIndex=1;
    entry=entry->root;
    }
  }

/*
*  Remove a cache row 
*/
int cacheRemove(nbCELL context,struct CACHE *cache,nbSET argSet){
  struct CACHE_TIMER *timer;
  struct CACHE_NODE *entry;
  struct CACHE_ATTR *attr;

  if((entry=cacheFindRow(context,cache,argSet,&attr))==NULL) return(0);
  cacheRemoveNode(context,cache,entry,attr);
  
  /* remove any timer elements pointing to removed entries - may be more than one */
  for(timer=cache->timer->next;timer!=cache->timer;timer=timer->next){
    if(timer->entry->object==NULL){  /* if in the free entry list */
      timer->prior->next=timer->next;
      timer->next->prior=timer->prior;
      timer->next=cacheTimerFree;   /* return to free list */
      timer->entry=NULL;
      cacheTimerFree=timer;
      timer=timer->prior;
      }
    }
  /* cancel alarm timer when the cash becomes empty */
  if(cache->interval && cache->timer->next==cache->timer){
    nbClockSetTimer(0,cache->node);
    cache->state&=0xff^CACHE_STATE_ALARM;
    }
  return(1);
  }

/*
*  Empty a cache - delete all counter entries and timer elements
*/
void cacheEmpty(nbCELL context,struct CACHE *cache){
  struct CACHE_TIMER *timer,*timerNext;
  struct CACHE_NODE *entry;
  
  //nbLogMsg(context,0,'T',"cacheEmpty called");
  if(cache==NULL || cache->entry==NULL) return;
  /* cancel alarm timer if set */
  if(cache->state&CACHE_STATE_ALARM){
    nbClockSetTimer(0,cache->node);
    cache->state&=0xff^CACHE_STATE_ALARM;
    }
  for(timer=cache->timer->next;timer!=cache->timer;timer=timerNext){
    timerNext=timer->next;
    timer->next=cacheTimerFree;   /* return to free list */
    timer->entry=NULL;
    cacheTimerFree=timer;
    }
  cache->timer->prior=cache->timer;
  cache->timer->next=cache->timer;
  entry=cache->entry;
  cacheEmptyNode(context,entry);
  entry->hits=0;
  entry->rows=0;
  entry->kids=0;
  entry->hitIndex=1;
  entry->rowIndex=1;
  entry->kidIndex=1;
  cache->state|=CACHE_STATE_PUBLISH;
  }

/* 
*  Free a cache - let nbNodeDestroy() do this
*/
void freeCache(nbCELL context,struct CACHE *cache){
  if(cache==NULL) return;
  cacheEmpty(context,cache);
  nbFree(cache,sizeof(struct CACHE));  
  }

/******************************************************
* Skill methods
******************************************************/
void *cacheConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbCELL cell;
  nbSET argSet;
  struct CACHE *cache=newCache(context,text);
  if(cache==NULL) return(NULL);
  argSet=nbListOpen(context,arglist);
  cell=nbListGetCell(context,&argSet);
  if(cell!=NULL){  // if we have a reset cell expression, then create synapse
    cache->releaseCell=cell;
    cache->releaseSynapse=nbSynapseOpen(context,skillHandle,cache,cache->releaseCell,cacheResetAlarm);
    if(NULL!=nbListGetCellValue(context,&argSet)){
      nbLogMsg(context,0,'E',"Cache skill only accepts one argument.");
      return(NULL);
      }
    }
  return(cache);
  }

int cacheAssert(nbCELL context,void *skillHandle,NB_Cache *cache,nbCELL arglist,nbCELL value){
  int mode=0;  // we only assert via standard interface
  nbSET argSet;
  int inserted=0,removed=0;  // flag indicated if known state was inverted

  //nbLogMsg(context,0,'T',"cacheAssert called");
  if(arglist==NULL) return(0); // perhaps we should set the value of the tree itself
  argSet=nbListOpen(context,arglist);
 
  cache->state&=0xff^(CACHE_STATE_PUBLISH&CACHE_STATE_ALERT);  // reset flags
 
  if(value!=NB_CELL_FALSE && value!=NB_CELL_UNKNOWN)
    inserted=cacheInsert(context,skillHandle,cache,cache->entry,argSet,cache->attr,mode);
  else if(argSet==NULL) cacheEmpty(context,cache);
  else removed=cacheRemove(context,cache,argSet);
  if(cache->state&CACHE_STATE_PUBLISH) nbCellPub(context,cache->context);
  if(cache->state&CACHE_STATE_ALERT) nbAction(context,cache->assertion,"",NB_CMDOPT_HUSH|NB_CMDOPT_ALERT);
  else if(cache->options&CACHE_OPTION_EXIST && inserted){
    //nbTermSetDefinition(context,cache->action,cache->addCell);
    nbAssertionAddTermValue(context,&cache->assertion,cache->action,cache->addCell);
    nbAction(context,cache->assertion,"",NB_CMDOPT_HUSH|NB_CMDOPT_ALERT);
    }
  else if(cache->options&CACHE_OPTION_EXIST && removed){
    //nbTermSetDefinition(context,cache->action,cache->deleteCell);
    nbAssertionAddTermValue(context,&cache->assertion,cache->action,cache->deleteCell);
    nbAction(context,cache->assertion,"",NB_CMDOPT_HUSH|NB_CMDOPT_ALERT);
    }
  else nbAction(context,cache->assertion,"",NB_CMDOPT_HUSH);
  cache->assertion=NULL;
  return(0);
  }

// This is just a test to see if the alert method is going to solve our problem
// If it works, break out a subroutine to be called by both cacheAssert and cacheAlert
// The only difference is the mode value call to contextAlert
int cacheAlert(nbCELL context,void *skillHandle,NB_Cache *cache,nbCELL arglist,nbCELL value){
  int mode=1;  /* we only assert via standard interface */
  nbSET argSet=nbListOpen(context,arglist);

  cache->state&=0xff^(CACHE_STATE_PUBLISH&CACHE_STATE_ALERT);  // reset flags

  if(value!=NB_CELL_FALSE && value!=NB_CELL_UNKNOWN){
    cacheInsert(context,skillHandle,cache,cache->entry,argSet,cache->attr,mode);
    }
  else if(arglist==NULL) cacheEmpty(context,cache);
  else cacheRemove(context,cache,argSet);
  if(cache->state&CACHE_STATE_PUBLISH) nbCellPub(context,cache->context);
  nbAction(context,cache->assertion,"",NB_CMDOPT_HUSH|NB_CMDOPT_ALERT);
  cache->assertion=NULL;
  return(0);
  }

nbCELL cacheEvaluate(nbCELL context,void *skillHandle,NB_Cache *cache,nbCELL arglist){
  nbSET argSet=nbListOpen(context,arglist);

  if(!argSet) return(NB_CELL_UNKNOWN); /* cache itself has no value */
  if(!cache) return(NB_CELL_UNKNOWN);
  if(cache->trace) nbLogMsg(context,0,'T',"cacheEvaluate: called"); // 2012-12-27 eat 0.8.13 - CID 76122 - must follow check for NULL cache
  if(arglist==NB_CELL_UNKNOWN) return(NB_CELL_UNKNOWN);
  if(cacheGetCount(context,cache,argSet,(int)0)) return(NB_CELL_TRUE);
  if(cache->trace) nbLogMsg(context,0,'T',"evalCache: returning false");
  return(NB_CELL_FALSE);
  }

void cacheSolve(nbCELL context,void *skillHandle,NB_Cache *cache,nbCELL arglist){
  nbCellSolve(context,arglist);
  return;
  }

void printCacheRows(nbCELL context,struct CACHE_NODE *entry,int column){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  int i;

  nbLogPut(context,"\n");
  NB_TREE_ITERATE(treeIterator,treeNode,(NB_TreeNode *)entry){
    entry=(struct CACHE_NODE *)treeNode;
    for(i=0;i<column;i++) nbLogPut(context,"  ");
    nbCellShow(context,entry->object);
    nbLogPut(context,"(%d:%d)",entry->hits,entry->hitIndex);
    //if(entry->entry!=NULL){
    if(entry->flags^CACHE_NODE_FLAG_LASTCOL){
      nbLogPut(context,"{%d:%d}",entry->rows,entry->rowIndex);
      nbLogPut(context,"[%d:%d],",entry->kids,entry->kidIndex);
      printCacheRows(context,entry->entry,column+1);
      }
    else nbLogPut(context,"\n");
    NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
    }
  }
/*
*  Show cache
*/
static int cacheShow(nbCELL context,void *skillHandle,NB_Cache *cache,int option){
//  struct CACHE_NODE *entry;
  struct CACHE_ATTR  *attr=cache->attr;
  int i;
 
  if(option==NB_SHOW_REPORT) nbLogPut(context,"  Specification: ");
  if(cache->releaseCell){
    nbLogPut(context,"(");
    nbCellShow(context,cache->releaseCell);
    nbLogPut(context,")");
    }
  nbLogPut(context,":(~(%ds)",cache->interval);
  for(attr=cache->attr;attr!=NULL;attr=attr->next){
    if(attr->term!=NULL && attr!=cache->attr) nbLogPut(context,nbTermGetName(context,attr->term));
    if(attr->hitThresh[1]!=0){
      nbLogPut(context,"(^%d",attr->hitThresh[0]);
      for(i=1;i<=CACHE_THRESHOLD_INDEX_LIMIT && attr->hitThresh[i]!=0;i++)
        nbLogPut(context,",%d",attr->hitThresh[i]);
      nbLogPut(context,")");
      }
    if(attr->rowThresh[1]!=0){
      nbLogPut(context,"{^%d",attr->rowThresh[0]);
      for(i=1;i<=CACHE_THRESHOLD_INDEX_LIMIT && attr->rowThresh[i]!=0;i++)
        nbLogPut(context,",%d",attr->rowThresh[i]);
      nbLogPut(context,"}");
      }
    if(attr->kidThresh[1]!=0){
      nbLogPut(context,"[^%d",attr->kidThresh[0]);
      for(i=1;i<=CACHE_THRESHOLD_INDEX_LIMIT && attr->kidThresh[i]!=0;i++)
        nbLogPut(context,",%d",attr->kidThresh[i]);
      nbLogPut(context,"]");
      }
    if(attr->next!=NULL){
      if(attr==cache->attr) nbLogPut(context,":");
      else nbLogPut(context,",");
      }
    } 
  nbLogPut(context,")");
  if(option==NB_SHOW_REPORT){
    nbLogPut(context,"\n  Options: Expire=%d Count=%d",(cache->options&CACHE_OPTION_EXPIRE)>0,(cache->options&CACHE_OPTION_COUNT)>0);
    nbLogPut(context,"\n  Status:  Alert=%d  Publish=%d\n  Elements:",(cache->state&CACHE_STATE_ALERT)>0,(cache->state&CACHE_STATE_PUBLISH)>0);
    nbLogFlush(context);
    printCacheRows(context,cache->entry,2);    
    }
  return(0);
  }


void *cacheDestroy(nbCELL context,void *skillHandle,NB_Cache *cache,int option){
  freeCache(context,cache);
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *cacheBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  struct CACHE_SKILL *skillHandle=nbAlloc(sizeof(struct CACHE_SKILL));

  skillHandle->unknown=nbCellCreate(context,"?");
  skillHandle->stateVal[0]=nbCellCreateString(context,"normal");
  skillHandle->stateVal[1]=nbCellCreateString(context,"minor");
  skillHandle->stateVal[2]=nbCellCreateString(context,"major");
  skillHandle->stateVal[3]=nbCellCreateString(context,"critical");

  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,cacheConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ASSERT,cacheAssert);
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,cacheEvaluate);
  nbSkillSetMethod(context,skill,NB_NODE_SOLVE,cacheSolve); /* do we need this? */
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,cacheShow);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,cacheDestroy);
  nbSkillSetMethod(context,skill,NB_NODE_ALARM,cacheAlarm);
  nbSkillSetMethod(context,skill,NB_NODE_ALERT,cacheAlert);
  return(skillHandle);
  }
