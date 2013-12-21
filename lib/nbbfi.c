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
* File:     nbbfi.c
*
* Title:    Binary Function of Integer (bfi) Routines
*
* Function:
*
*   This file provides routines that implement NodeBrain "Binary Function
*   of Integer (bfi)" operations.  These routines are used to implement the
*   NodeBrain specification for scheduling.  However, these routines are more
*   abstract, with possible application for binary functions of integers 
*   which represent something other than time.
*
* Synopsis:
*
*   #include "nb.h"
*
*       Administrative functions
*
*   void bfiInsert(bfi f, long start, long end);
*   bfi bfiRemove(bfi s);
*
*   bfi bfiNew(long start, long end);
*   bfi bfiDomain(bfi g, bfi h);
*   bfi bfiCopy(bfi f);
*   bfi bfiDispose(bfi f);
*
*       Debugging functions
*
*   void bfiPrint(bfi f, char *label);
*   bfi bfiParse(char *s);
*   int bfiCompare(bfi g, bfi h);
*   int bfiEval(bfi f, int i);
*
*       Boolean Operations
*
*   bfi bfiAnd_(bfi g);          Overlap
*   bfi bfiNot_(bfi g);          Not 
*   bfi bfiOr_(bfi g);           Connect            
*   bfi bfiOre_(bfi g);          Normalize    
*   bfi bfiXor_(bfi g);          Connect/Unique
*   bfi bfiXore_(bfi g);         Unique
*
*   bfi bfiAnd(bfi g, bfi h);
*   bfi bfiOr(bfi g, bfi h);
*   bfi bfiOre(bfi g, bfi h);
*   bfi bfiXor(bfi g, bfi h);
*   bfi bfiXore(bfi g, bfi h);
*
*       Segment Operations
*
*   bfi bfiKnown(bfi s);
*   bfi bfiUntil_(bfi g);        Partition           
*   bfi bfiYield_(bfi g);
*
*   bfi bfiIndex(bfi g, bfiindex *index);  ???   Count
*
*   bfi bfiReject(bfi g, bfi h); Reject
*   bfi bfiSelect(bfi g, bfi h); 
*   bfi bfiUnion(bfi g, bfi h);  Union   
*   bfi bfiUntil(bfi g, bfi h);  Until
*   bfi bfiYield(bfi g, bfi h);
*
*
* Description:
*
*   We define a binary function of integer as follows.
*
*      Let f be a binary function of integer.
*      Let i be an integer.
*      Let a be the lower bound on the domain of f.
*      Let b be the upper bound on the domain of f.
*      f(i) = 0 or 1 if, and only if, a<=i<b.
*      f(i) = -1 if, and only if, i<a or i>=b.      
*
*         0 represents FALSE
*         1 represents TRUE
*        -1 represents UNKNOWN
*
*   We implement a binary function of integer using a double linked
*   list data structure.  Each list entry represents a line segment
*   over which the function is true.
*
*      struct bfiseg {       // f(i) is 1 for any i where start<=i<end.
*        struct bfiseg *prior;
*        struct bfiseg *next;
*        long start;         // inclusive             
*        long end;           // exclusive
*        };
*
*   We illustrate a segment with a "|" for the start value and a "-" for
*   each consecutive integer less than the end value.
*
*        |-----------------     |-------- 
*
*   The bfi data type is defined as a pointer to a bfiseg structure.
*
*      deftype struct bfiseg *bfi;
*
*   A binary function of integer, f, can be declared as follows.
*
*      bfi f;
*
*   The segment directly pointed to by a bfi variable defines the domain
*   over which f(i) is known.  This segment acts like a first and last entry
*   in a circular list.  More specifically, it provides a first end, and a
*   last start.  Consequently, as a line segment, the values will seem
*   backwards.
*
*      f->start is the upper_bound b
*      f->end is the lower_bound a
*
*   We illustrate the domain segment with an "=" for each integer i, where
*   i is in the domain; that is, a<=i<b.
*   
*      =============================== 
*
*   Assuming we have some method of constructing the segments (bfiseg)
*   and properly linking them, we can evaluate f(i) for some value of i
*   as follows.
*
*      If i<=lower_bound or i>upper_bound then f(i)=-1;  (unknown)
*      Otherwise, if start<=i<end for at least one bfiseg associated
*                 with f, f(i)=1. (true)
*      Otherwise f(i)=0. (false)      
*
*   Given two binary functions of integer, g and h, we can implement
*   Boolean algebra operations to construct a new function f, such that
*
*      f(i) = g(i) <operation> h(i), for all i in the domain.
*
*   For the AND operation we would construct f, such that
*
*      f(i) = g(i) AND h(i), for all i in the domain.
*
*   This file provides C functions to manipulate bfi structures in this
*   way.  We use AND as an example again. The bfi function for AND is
*   bfiAnd.  
*    
*      bfi f,g,h;      // declare bfi variables
*      ...             // construct g and h bfi structures
*      f=bfiAnd(g,h);  // f(i) = g(i) AND h(i)
*
*   While many of the operations we support on bfi structures are based
*   on Boolean algebra, we include "set" operations based on "segment
*   relationships".  For example, we provide an operation which constructs
*   f, where f has all segments of g which intersect at least one segment
*   of h.  For this type of operation, one should think of a bfi as a set
*   of line segments.  The use of set operations to construct binary
*   functions of integer is what makes these routines powerful for
*   scheduling applications.
*
*   Our algorithms assure (and assume) that bfi list structures are properly
*   ordered.   
*
*   Naming conventions used in this code are:
*
*     Functions/sets :   f,g,h   
*     Segments:          s,t,l,r
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2000-06-09 Ed Trettevik (started original C prototype version)
* 2003-10-27 eat 0.5.5  Included bfiConflict_ function.
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-01-26 dtl 0.8.6  Checker updates
* 2012-08-31 dtl 0.8.12 Checker updates: handled malloc error
* 2012-12-27 eat 0.8.13 Checker updates
* 2012-12-31 eat 0.8.13 Checker updates
* 2013-01-13 eat 0.8.13 Checker updates - rosecheckers
* 2013-01-23 eat 0.8.13 Checker updates
* 2013-02-03 eat 0.8.13 Checker updates - CID 971426
*=============================================================================
*/
#include <nb/nbi.h>

/**************************************************************************
* Index Routines
* parse string format: from1_to1,from2_to2,..
* Return NULL if syntax error or input s too big
**************************************************************************/       


/*
*  Determine if character string is an integer
*
*  Returns:  1 - true (is integer), 0 - false (is not integer)
*/
int bfiIsInteger(char *cursor){
  if(*cursor=='-' || *cursor=='+') cursor++;
  while(isdigit(*cursor)) cursor++;
  if(*cursor) return(0);
  return(1);
  }

/*
*  Free index list
*/
void bfiIndexFree(struct bfiindex *top){
  struct bfiindex *entry;
  while(top){ // clean up list
    entry=top->next;
    nbFree(top,sizeof(struct bfiindex));
    top=entry;
    }
  }

/*
*  Verify number
*/

/*
*  Parse index specification
*
*  Syntax:
*
*     index   ::= element[[,element]...]
*
*     element ::=  n | n_n | n..n
*
*     n       ::= number
* 
*  Returns: Index structure list or NULL on syntax error.
*
*/
#define NB_BFI_INDEX_SIZE 64   // index element buffer size

struct bfiindex *bfiIndexParse(char *s,char *msg,size_t msglen){ // 2012-12-31 eat - VID 5210,5457,5136 - changed msglen from int to size_t
  int type=0,min=32000,max=-32000;
  struct bfiindex *top=NULL,*entry;  
  char *cursor=s,*comma,element[NB_BFI_INDEX_SIZE],sfrom[sizeof(element)],sto[sizeof(element)];
  size_t size;
    
  while(*cursor!=0){
    if((comma=strchr(cursor,','))==NULL) comma=cursor+strlen(cursor);
    size=comma-cursor;
    if(size>=sizeof(element)){
      snprintf(msg,msglen,"Index element exceeds maximum size of %d characters at: %s",NB_BFI_INDEX_SIZE-1,cursor);
      bfiIndexFree(top);
      return(NULL);
      }
    strncpy(element,cursor,size);
    *(element+size)=0;
    if((cursor=strchr(element,'_'))!=NULL){ //if '_' found
      type=Span;
      *cursor=0;
      cursor++;
      strcpy(sfrom,element); // safe - ignore Checker
      if(strlen(cursor)>=sizeof(sto)){
        snprintf(msg,msglen,"bfiIndexParse: Logic error - element should not be larger than sto");
        bfiIndexFree(top);
        return(NULL);
        }
      strncpy(sto,cursor,sizeof(sto));  // strcpy would be fine, but help the checker
      *(sto+sizeof(sto)-1)=0;           // 2013-01-23 eat - VID 6827 FP in other application but help the checker more
      }
    else if((cursor=strstr(element,".."))!=NULL){ //if ".." found
      type=Range;
      *cursor=0;
      cursor+=2;
      strcpy(sfrom,element); // safe - ignore Checker
      if(strlen(cursor)>=sizeof(sto)){
        snprintf(msg,msglen,"bfiIndexParse: Logic error - element should not be larger than sto");
        bfiIndexFree(top);
        return(NULL);
       }
      strncpy(sto,cursor,sizeof(sto));  // strcpy would be fine, but help the checker
      *(sto+sizeof(sto)-1)=0;             // 2013-01-23 eat - VID 6829 FP in other application but help the checker more
      }
    else{
      type=Simple;
      strcpy(sfrom,element); // safe - ignore Checker
      strcpy(sto,element);   // safe - ignore Checker
      }      
    if(!bfiIsInteger(sfrom) || !bfiIsInteger(sto)){
      snprintf(msg,msglen,"Index element \"%s\" has non-integer component",element);
      bfiIndexFree(top);
      return(NULL);
      }
    entry=(struct bfiindex *)nbAlloc(sizeof(struct bfiindex));
    entry->type=type;
    if((entry->from=atoi(sfrom))<min) min=entry->from;
    if(entry->from>max) max=entry->from;    
    if((entry->to=atoi(sto))>max) max=entry->to;
    if(entry->to<min) min=entry->to;
    entry->next=top;                        /* insert in list */
    top=entry;
    cursor=comma;
    if(*cursor) cursor++;
    }  
  entry=(struct bfiindex *)nbAlloc(sizeof(struct bfiindex));  /* insert the min/max entry */ 
  entry->type=type;
  entry->from=min;
  entry->to=max;
  entry->next=top;
  top=entry;  
  return(top);        
  }
  
void bfiIndexPrint(struct bfiindex *index){
  struct bfiindex *cursor;

  printf("[");  
  for(cursor=index;cursor!=NULL;cursor=cursor->next){
    if(cursor!=index) printf(",");
    if(cursor->type==Simple)     printf("%d",cursor->from);
    else if(cursor->type==Range) printf("%d..%d",cursor->from,cursor->to);
    else if(cursor->type==Span)  printf("%d_%d",cursor->from,cursor->to);
    }
  printf("]\n");  
  }
  
/*************************************************************************
* Segment free list
*************************************************************************/       
bfi bfifree=NULL;

/*************************************************************************
*  Administrative Functions
*************************************************************************/

/*
*  Allocate a block of 256 free segment cells and return 1.
*/
bfi bfiAlloc(void){
  bfi s;
  
  bfifree=(bfi)malloc(256*sizeof(struct bfiseg));
  if(!bfifree){  //2012-08-31 dtl - handle malloc() err
    fprintf(stderr,"Out of memory\n");
    exit(NB_EXITCODE_FAIL);
    }
  for(s=bfifree;s<bfifree+254;s++){
    s->next=s+1;
    }
  s->next=NULL;
  s++;
  return(s);
  }
  
/* 
*  Allocate a new function/set (domain segment)
*/
bfi bfiNew(long start,long end){
  bfi f=bfifree;

  if(f==NULL) f=bfiAlloc();
  else bfifree=bfifree->next;
  f->prior=f;
  f->next=f;
  if(end>start){
    f->start=end;
    f->end=start;
    }
  else{
    f->start=start;
    f->end=end;
    }
  return(f);
  }
  
/*
*  Allocate a new function/set domain based on two existing sets.
*
*/
bfi bfiDomain(bfi g,bfi h){
  long start,end;
  
  if(g->end>=h->end) start=g->end;     /* maximum start */
  else start=h->end;
  if(g->start<=h->start) end=g->start; /* minimum end */
  else end=h->start;
  if(end<start) end=start;             /* f(i) is unknown for all i */  
  return(bfiNew(start,end));
  }

/* 
*  Add a segment to a function/set
*/
void bfiInsert(bfi f,long start,long end){
  bfi s,t;
  
  for(s=f->prior;s!=f && (start<s->start || (start==s->start && end<s->end));s=s->prior);
/*  if(start==s->start && end==s->end) return; */
  if((t=bfifree)==NULL) t=bfiAlloc();
  else bfifree=bfifree->next;
  t->prior=s;
  t->next=s->next;
  t->start=start;
  t->end=end;
  s->next->prior=t;
  s->next=t;
  return;
  }
 
/* 
*  Add a unique segment to a function/set
*/
void bfiInsertUnique(bfi f,long start,long end){
  bfi s,t;
  
  for(s=f->prior;s!=f && (start<s->start || (start==s->start && end<s->end));s=s->prior);
  if(start==s->start && end==s->end) return;
  if((t=bfifree)==NULL) t=bfiAlloc();
  else bfifree=bfifree->next;
  t->prior=s;
  t->next=s->next;
  t->start=start;
  t->end=end;
  s->next->prior=t;
  s->next=t;
  return;
  }
 
/* 
*  Remove a segment from a function/set and return prior segment.
*/
bfi bfiRemove(bfi s){
  bfi prior=s->prior;

  s->prior->next=s->next;
  s->next->prior=s->prior;
  s->next=bfifree;
  bfifree=s;
  return(prior);
  }  

/*
*  Dispose of a function/set. 
*/
bfi bfiDispose(bfi f){
 
  if(f==NULL) return(NULL);
  if(f->prior==NULL) return(NULL);
  f->prior->next=bfifree;
  bfifree=f;
  return(NULL);
  }
  
/*
*  Copy a function/set.
*
*/
bfi bfiCopy(bfi g){
  bfi f,s;
  
  f=bfiNew(g->start,g->end);
  for(s=g->next;s!=g;s=s->next){
    bfiInsert(f,s->start,s->end);
    }
  return(f);
  }
        
  
/*************************************************************************
*  Debugging Functions
*************************************************************************/
/*
*  Evaluate a bfi for a given integer
*/
int bfiEval(bfi f,long i){
  bfi s;

  if(i<=f->end || i>f->start) return(0);  
  for(s=f->next;s!=f;s=s->next){
    if(i<s->start) return(0);
    if(i<s->end) return(1);
    }
  return(0);
  }
  
/*
*  Compare two functions/sets
*/
int bfiCompare(bfi g,bfi h){
  bfi s,t=h->next;
  
  if(g->start!=h->start || g->end!=h->end) return(0);
  for(s=g->next;s!=g;s=s->next){
    if(t==h || s->start!=t->start || s->end!=t->end) return(0);
    t=t->next;
    }
  if(t!=h) return(0);
  return(1);
  }
    
/*
*  Print a function/set for debugging
*/
void bfiPrint(bfi f,char *label){
  bfi s=f;

  printf("%s=(%ld_%ld:",label,f->end,f->start-1);
  for(s=f->next;s!=f;s=s->next){
    if(s!=f->next) printf(",");
    if(s->start==s->end-1) printf("%ld",s->start);
    else printf("%ld_%ld",s->start,s->end-1);
    } 
  //printf(");\n",f->start);
  printf(");\n");
  }

/*
*  Parse a bfi definition string for debugging
*
*  Returns: bfi or NULL if syntax error
*/

bfi bfiParse(char *s){
  bfi f;
  char *comma,*colon,*bar;
  long start,end;
  
  if((colon=strchr(s,':'))==NULL) return(NULL); // 2012-01-25 eat - added return
  *colon=0; 
  if((bar=strchr(s,'_'))==NULL) return(NULL);   // 2012-01-25 eat - added return
  *bar=0;
  start=atoi(s);
  end=atoi(bar+1)+1;
  s=colon+1;
  f=bfiNew(start,end);
  while(*s!=0){
    comma=strchr(s,',');
    if(comma!=NULL) *comma=0;
    else comma=s+strlen(s)-1;
    bar=strchr(s,'_');
    if(bar!=NULL){
      *bar=0;
      start=atoi(s);
      s=bar+1;
      end=atoi(s)+1;
      }
    else{
      start=atoi(s);
      end=start+1;
      }
    bfiInsert(f,start,end);  
    s=comma+1;
    }
  return(f);
  }
  
/*************************************************************************
*  Single Set Segment Operations
*************************************************************************/
/*
*  Known: 
* 
*    Input      ================================= 
*         |-- |------    |--------    |---   |---------   |------ 
*                |-  |-      |------        |-- 
*
*    Result     ================================= 
*               |----    |--------    |---   |--- 
*                |-  |-      |------        |-- 
*/
bfi bfiKnown(bfi g){
  bfi f,s;  

  f=bfiNew(g->start,g->end);     
  for(s=g->next;s!=g && s->start<=g->end;s=s->next){
    if(s->end>g->end){
      if(s->end<=g->start) bfiInsert(f,g->end,s->end);
      else bfiInsert(f,g->end,g->start);
      }
    }
  for(;s!=g && s->end<g->start;s=s->next){
    bfiInsert(f,s->start,s->end);
    }
  for(;s!=g && s->start<g->start;s=s->next){
    bfiInsert(f,s->start,g->start);
    }
  return(f);
  }
  
/*
*  Until: (Partition)
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                            |------  |--- 
*
*    Result     ================================= 
*             |----------|---|--------|------|------------|------ 
*
*    Input      ================================= 
*                        |--------    |---
*                            |------  |--- 
*
*    Result     ================================= 
*               |--------|---|--------|---------- 
*/
bfi bfiUntil_(bfi g){
  bfi f,s;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g){
    bfiInsert(f,g->end,g->start);
    return(f);
    }
  if(g->end<g->next->start) bfiInsert(f,g->end,g->next->start); /* fill domain */
  for(s=g->next;s->next!=g;s=s->next){
    if(s->start<s->next->start) bfiInsert(f,s->start,s->next->start);
    }
  if(s->end<g->start) bfiInsert(f,s->start,g->start);  /* fill domain */
  else bfiInsert(f,s->start,s->end);
  return(f);
  }
  
/*
*  Yield:
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                            |------  |--- 
*
*    Result     ================================= 
*             |------    |---|------  |---   |---------   |------ 
*/
bfi bfiYield_(bfi g){
  bfi f,s;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g) return(f);
  for(s=g->next;s->next!=g;s=s->next){
    if(s->start<s->next->start){
      if(s->end>s->next->start) bfiInsert(f,s->start,s->next->start);
      else bfiInsert(f,s->start,s->end);
      }
    }
  bfiInsert(f,s->start,s->end);
  return(f);
  }

/*
*  Conflict: 
*
*    Input      ================================= 
*             |---  |--------    |---   |---------   |------ 
*                          |------    |-            |--------- 
*
*    Result     ================================= 
*                   |--------    |---                |------ 
*                          |------                  |--------- 
*                            
*/
bfi bfiConflict_(bfi g){
  bfi f,s,t;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g) return(f);
  for(s=g->next;s->next!=g;s=s->next){
    if((t=s->next)!=g && t->start<s->end)
      bfiInsertUnique(f,s->start,s->end);
    for(t=s->next;t!=g && t->start<s->end;t=t->next)
      bfiInsertUnique(f,t->start,t->end);
    } 
  return(f);
  }
           
/*************************************************************************
*  Single Set Boolean Operations
*************************************************************************/
/*
*  Or_: Boolean "or"
*
*    Input      ================================= 
*             |------    |----|---    |---|--   |---------|------ 
*                |--                              |-----
*
*    Result     ================================= 
*             |------    |--------    |------   |---------------- 
*/
bfi bfiOr_(bfi g){
  bfi f,s=g->next;
  
  f=bfiNew(g->start,g->end);
  if(s==g) return(f);
  bfiInsert(f,s->start,s->end);
  for(s=s->next;s!=g;s=s->next){
    if(f->prior->end<s->start)
      bfiInsert(f,s->start,s->end);
    else if(f->prior->end<s->end) f->prior->end=s->end;
    }
  return(f);
  }
        
/*
*  Ore_: Boolean "or" with "edge preservation"
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                |-  |-      |------        |-- 
*
*    Result     ================================= 
*             |------|-  |----------  |---  |----------   |------ 
*/
bfi bfiOre_(bfi g){
  bfi f,s=g->next;
  
  f=bfiNew(g->start,g->end);
  if(s==g) return(f);
  bfiInsert(f,s->start,s->end);
  for(s=s->next;s!=g;s=s->next){
    if(f->prior->end<=s->start)
      bfiInsert(f,s->start,s->end);
    else if(f->prior->end<s->end) f->prior->end=s->end;
    }
  return(f);
  }
        
/*
*  And_: Boolean "and"
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                            |------                |--------- 
*
*    Result     ================================= 
*                            |----                  |--   |--- 
*/
bfi bfiAnd_(bfi g){
  bfi f,h,s,t;
  long end;
  
  h=bfiNew(g->start,g->end);
  if(g->next==g) return(h);
  for(s=g->next;s->next!=g;s=s->next){
    for(t=s->next;t!=g && t->start<s->end;t=t->next){
      if(s->end<t->end) end=s->end;
      else end=t->end;
      bfiInsert(h,t->start,end);
      }
    }
  f=bfiOr_(h);
  bfiDispose(h);  
  return(f);
  }
  
/*
*  Not: Boolean "not"
*
*    Input      ================================= 
*                        |--------    |---                |------ 
*                 |----      |------        |-- 
*
*    Result     ================================= 
*               |-     |-           |-         |- 
*
*  NOTE: The NOT operation works within the defined domain only.
*/
bfi bfiNot_(bfi g){
  bfi f,h,s;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g) return(f);
  h=bfiOr_(g);
  if(h->end<h->next->start){
    bfiInsert(f,h->end,h->next->start);
    }
  for(s=h->next;s!=h && s->end<h->start;s=s->next){
    if(s->end<s->next->start){ 
      bfiInsert(f,s->end,s->next->start);
      }
    }
  bfiDispose(h);  
  return(f);
  }
        
/*
*  Xor_: Boolean "exclusive or"
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                    |--     |------                |--------- 
*
*    Result     ================================= 
*             |--------- |---     |-  |---   |------   |--    |-- 
*/
bfi bfiXor_(bfi g){
  bfi f,s;
  long start=g->next->start;
  long end=g->next->end;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g) return(f);
  for(s=g->next->next;s!=g;s=s->next){
    if(end==s->start) end=s->end;      /* connect adjoining segments */
    else if(end<s->start){             /* non-overlapping segments */
      bfiInsert(f,start,end);
      start=s->start;
      end=s->end; 
      }
    else if(start<s->start){           /* overlapping segments */
      bfiInsert(f,start,s->start);
      if(end>s->end) start=s->end;
      else{
        start=end;
        end=s->end;
        }
      }
    }
  if(end>start) bfiInsert(f,start,end);   
  return(f);
  }  

/*
*  Xore: Boolean "exclusive or" with "edge preservation"
*
*    Input      ================================= 
*             |------    |--------    |---   |---------   |------ 
*                    |-      |------                |--------- 
*
*    Result     ================================= 
*             |------|-  |---     |-  |---   |------   |--    |-- 
*/
bfi bfiXore_(bfi g){
  bfi f,s;
  long start=g->next->start;
  long end=g->next->end;
  
  f=bfiNew(g->start,g->end);
  if(g->next==g) return(f);
  for(s=g->next->next;s!=g;s=s->next){
    if(end<=s->start){                 /* non-overlapping segments */
      bfiInsert(f,start,end);
      start=s->start;
      end=s->end; 
      }
    else if(start<s->start){           /* overlapping segments */
      bfiInsert(f,start,s->start);
      if(end>s->end) start=s->end;
      else{
        start=end;
        end=s->end;
        }
      }
    }
  if(end>start) bfiInsert(f,start,end);   
  return(f);
  }  

/*
*  bfiIndexOne:
*
*    Input        ================================= 
*               |------    |--------    |---   |---------   |------ 
*
*    Result n=3   ================================= 
*                                       |--- 
*
*    Result n=-1  ================================= 
*                                              |--------- 
*/
// 2012-12-27 eat 0.8.13 - CID 751538 - fixed defect that resulted in dead code
bfi bfiIndexOne(bfi g,int i){
  bfi f,s;

  f=bfiNew(g->start,g->end); 
  if(i>0){  
    for(s=g->next;s!=g && s->end<=g->end;s=s->next);  // ignore below range - ending before start (g->end)
    for(;s!=g && s->start<g->start && i>0;s=s->next,i--); // count down while within range
    }
  else if(i<0){ 
    for(s=g->prior;s!=g && s->start>=g->start;s=s->prior); // ignore above range - start after end (>g->start) 
    for(;s!=g && s->start>=g->end && i<0;s=s->prior,i++);  // count up while within range
    }
  else return(f);
  if(i==0 && s->start<g->start && s->end>g->end){  // insert if intersecting
    bfiInsert(f,s->start,s->end);
    return(f);
    }
  return(f);
  }
  
/*
*  bfiIndex:  (prototype)
*
*    Input        ================================= 
*               |------    |--------    |---   |---------   |------ 
*
*    Result n=3   ================================= 
*                                       |--- 
*
*    Result n=-1  ================================= 
*                                              |--------- 
*/
bfi bfiIndex(bfi g,struct bfiindex *index){
  int J,K,n=0;
  bfi f,s,*array,*Jseg,*Kseg;
  struct bfiindex *range;

  f=bfiNew(g->start,g->end);
  if(index==NULL) return(f);
  /* build an index */ 
  if(index->from>0){     /* we can bound the indexing by the maximum index */
    int stop=index->to;
    array=(bfi *)malloc(sizeof(bfi)*stop);
    if(!array) nbExit("bfiIndex: out of memory");
    for(s=g->next;s!=g && n<stop && s->start<g->start;s=s->next) if(s->end>g->end){
      *(array+n)=s;
      n++;
      }
    }
  else{                  /* we need to index the complete list */ 
    int entries=500;
    array=(bfi *)malloc(sizeof(bfi)*entries);
    if(!array) nbExit("bfiIndex: out of memory");
    for(s=g->next;s!=g && s->start<g->start;s=s->next) if(s->end>g->end){
      *(array+n)=s;
      n++;
      if(n>=entries){    /* this is a big set */
        entries*=2;      /* double the number of entries */ 
        array=(bfi *)realloc(array,sizeof(bfi)*entries);
        if(!array) nbExit("bfiIndex: out of memory");  // 2012-12-31 eat - VID 5135 - error check
        }
      }
    }
  for(range=index->next;range!=NULL;range=range->next){ 
    J=range->from;
    K=range->to; 
    if(J<0) J=J+n;
    else J=J-1;
    if(K<0) K=K+n;
    else K=K-1;
    if(K>=n) K=n-1;
    if(J<0) J=0; 
    if(J<n && K>=0 && K>=J){   
      Jseg=array+J;
      Kseg=array+K+1;
      if(range->type==Span){  /* note: we are trusting that spans are truly spans */
        long start=((bfi)*Jseg)->start;
        long end=((bfi)*Jseg)->end;
        for(;Jseg!=Kseg;Jseg++){
          s=*Jseg;
          if(s->end>end) end=s->end;
          }
        bfiInsert(f,start,end);  
        }
      else{
        for(;Jseg!=Kseg;Jseg++){
          s=*Jseg;
          bfiInsert(f,s->start,s->end); 
          }
        }
      }
    }
  free(array);
  return(f);
  }

/*************************************************************************
* Two Set Segment Operations
*************************************************************************/ 
/*
*  Reject: 
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                        |-----            |---------   |------ 
*
*    Result     ================================= 
*             |------                 |--- 
*/
bfi bfiReject(bfi g,bfi h){
  bfi f,s=g->next,t,H;

  if(s==g || h->next==h) return(bfiCopy(g));
  f=bfiDomain(g,h);
  H=bfiOr_(h);
  t=H->next;
  while(s!=g && t!=H){
    while(s->end<=t->start && t->prior!=H) t=t->prior;
    while(s->start>=t->end && t->next!=H) t=t->next;
    if(s->start>=t->end || s->end<=t->start){
      bfiInsert(f,s->start,s->end);
      }
    s=s->next;
    }
  bfiDispose(H);
  return(f);
  }  

/*
*  Select: 
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                        |-----            |---------   |------ 
*
*    Result     ================================= 
*                        |--------           |---------   |------ 
*/  
bfi bfiSelect(bfi g,bfi h){
  bfi f,s=g->next,t,H;
  
  f=bfiDomain(g,h);
  H=bfiOr_(h);
  t=H->next;
  while(s!=g && t!=H){
    while(s->start>=t->end && t->next!=H) t=t->next;
    if(s->start<t->end && s->end>t->start){
      bfiInsert(f,s->start,s->end);
      }
    s=s->next;
    }
  bfiDispose(H);
  return(f);
  }  

/*
*  IndexedSelect: 
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                        |-----            |---------   |------ 
*
*    Result     ================================= 
*                        |--------           |---------   |------ 
*/  
bfi bfiIndexedSelect(bfi g,bfi h,struct bfiindex *index){
  bfi f,F,s,t;
  long start=g->start,end=g->end;
  
  f=bfiDomain(g,h);
  for(s=h->next;s!=h;s=s->next) if(s->start<start && s->end>end){
    g->start=s->end;
    g->end=s->start;
    F=bfiIndex(g,index);
    for(t=F->next;t!=F;t=t->next){
      bfiInsertUnique(f,t->start,t->end);
      }
    // F=bfiDispose(F);  // 2012-12-18 eat - CID 751690
    bfiDispose(F);
    }
  g->start=start;
  g->end=end;
  return(f);
  }  

/*
*  Union: (logical "or" in unnormalized form)
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*                            |------                |--------- 
*    Input 2:   ================================= 
*                        |-----    |---   |---------   |------ 
*                            |------            |--------- 
*
*    Result     ================================= 
*             |------    |--------    |---   |---------   |------ 
*                            |------                |--------- 
*                        |-----    |---   |---------   |------ 
*                            |------            |--------- 
*/
bfi bfiUnion(bfi g,bfi h){
  bfi f,s=g->next,t=h->next;
  
  f=bfiDomain(g,h);
  while(s!=g && t!=h){
    if(s->start<t->start){
      bfiInsert(f,s->start,s->end);
      s=s->next;
      }
    else if(s->start>t->start){
      bfiInsert(f,t->start,t->end);
      t=t->next;
      }
    else{  /* s->start==t->start */
      if(s->end<=t->end){
        bfiInsert(f,s->start,s->end);
        s=s->next;
        }
      else{
        bfiInsert(f,t->start,t->end);
        t=t->next;
        }
      }
    }
  while(s!=g){
    bfiInsert(f,s->start,s->end);
    s=s->next;
    }
  while(t!=h){
    bfiInsert(f,t->start,t->end);
    t=t->next;
    }
  return(f);
  }  

/*
*  Until: 
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                        |-----            |---------   |------ 
*
*    Result     ================================= 
*             |----------             |----  |---------- 
*                        |----------------- 
*/
bfi bfiUntil(bfi g,bfi h){
  bfi f,s=g->next,t=h->next;
  
  f=bfiDomain(g,h);
  while(s!=g){
    while(s->start>=t->start){
      if(t==h) return(f);
      t=t->next;
      }
    if(t!=h) bfiInsert(f,s->start,t->start);
    else bfiInsert(f,s->start,s->end);
    s=s->next;
    }
  return(f);
  }  

/*
*  Yield: 
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                  |---  |-----            |------   |------ 
*
*    Result     ================================= 
*             |----      |--------    |---   |-------     |------
*/
bfi bfiYield(bfi g,bfi h){
  bfi f,s=g->next,t=h->next;
  
  f=bfiDomain(g,h);
  while(s!=g){
    while(s->start>=t->start){
      if(t==h) return(f);
      t=t->next;
      }
    if(t!=h && s->end>t->start) bfiInsert(f,s->start,t->start);
    else bfiInsert(f,s->start,s->end);
    s=s->next;
    }
  return(f);
  }  

/*************************************************************************
* Two Set Boolean Operations
*************************************************************************/ 
/*
*  And: (logical "and")
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                    |-  |-----    |---     |-----   |------ 
*
*    Result     ================================= 
*                        |-----       |      |----   |-   |- 
*/
bfi bfiAnd(bfi g,bfi h){
  bfi f,F,G,H;

  G=bfiOr_(g);
  H=bfiOr_(h);
  F=bfiUnion(G,H);  
  bfiDispose(G);
  bfiDispose(H);
  f=bfiAnd_(F);
  bfiDispose(F);
  return(f);
  }  

/*
*  Or: (logical "or")
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                    |-  |-----    |---     |-----   |------ 
*
*    Result     ================================= 
*             |------    |-------- |------  |-------------------- 
*                    |- 
*/
bfi bfiOre(bfi g,bfi h){
  bfi f,F;

  F=bfiUnion(g,h);  
  f=bfiOre_(F);
  bfiDispose(F);
  return(f);
  }  

/*
*  Orr: (logical "or" in reduced form)
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                    |-  |-----    |---     |-----   |------ 
*
*    Result     ================================= 
*             |--------  |-------- |------  |-------------------- 
*/
bfi bfiOr(bfi g,bfi h){
  bfi f,F;

  F=bfiUnion(g,h);  
  f=bfiOr_(F);
  bfiDispose(F);
  return(f);
  }  

/*
*  Xor: (logical "xor")
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                    |-  |-----    |---     |-----   |------ 
*
*    Result     ================================= 
*             |--------        |-- |-- |--  |     |--  |--  |---- 
*/
bfi bfiXor(bfi g,bfi h){
  bfi f,F,G,H;

  G=bfiOr_(g);
  H=bfiOr_(h);
  F=bfiUnion(G,H);  
  bfiDispose(G);
  bfiDispose(H);
  f=bfiXor_(F);
  bfiDispose(F);
  return(f);
  }  

/*
*  Xore: (logical "xor" preserving edges)
*
*    Input 1:   ================================= 
*             |------    |--------    |---   |---------   |------ 
*    Input 2:   ================================= 
*                    |-  |-----    |---     |-----   |------ 
*
*    Result     ================================= 
*             |------|-        |-- |-- |--  |     |--  |--  |---- 
*/
bfi bfiXore(bfi g,bfi h){
  bfi f,F,G,H;

  G=bfiOre_(g);
  H=bfiOre_(h);
  F=bfiUnion(G,H);  
  bfiDispose(G);
  bfiDispose(H);
  f=bfiXore_(F);
  bfiDispose(F);
  return(f);
  }
