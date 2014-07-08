/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbregex.h 
*
* Title:    Regular Expression Object Management Routines (prototype)
*
* Function:
*
*   This header provides routines that manage nodebrain regular expression
*   objects.  We use standard regular expression as supported by regex.h, but
*   package them here to fit into the nodebrain object scheme.
*  
* Synopsis:
*
*   #include "nb.h"
*
*   void initRegexp();
*
*   struct REGEXP *newRegexp(char *expression,int flags);
*
* Description
*
*   You can then construct a regular expression using the newRegexp() method.
*
*     struct REGEXP *myregexp=newRegexp(expression,msg);
*
*   The caller is responsible for incrementing the use count of a regular
*   expression when references are assigned.  A call to grabObject() for the
*   regular expression returned might look like this.
*
*     mypointer=grabObject(myregexp);
*
*   A call to dropObject is required when the pointer is changed.
*
*     dropObject(struct REGEXP *myregexp);
*
*   A regular expression is treated as a constant and does not publish changes.
*    
*===============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ------------------------------------------------------------------
* 2001-07-15 eat 0.2.8  Pulled from main routine.
* 2003-03-15 eat 0.5.1  Split *.h and *.c for new make file
* 2008-10-06 eat 0.7.2  Included support for regex compile flags
* 2008-11-06 eat 0.7.3  Converted to PCRE's native API
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*===============================================================================
*/
#include <nb/nbi.h>

struct HASH *regexpH;      /* regular expression hash */
struct TYPE *regexpType;   /* regular expression object type */
struct REGEXP *freeRegexp; /* free structure pool */

/*
*  Hash a regular expression and return a pointer to a pointer in the vector.
*/
static void *hashRegexp(struct HASH *hash,struct STRING *value,int flags){ 
  unsigned long *h,k;
  h=(unsigned long *)&value;
  k=*h;
  k+=flags;
  return(&(hash->vect[k%hash->modulo]));
  }

/*******************************************************************************
*  Regular Expression Methods
*******************************************************************************/
/*
* Maintain a unique set of regular expressions
*
*   REG_EXTENDED|REG_NOSUB   - used for matching only (cell expressions)
*   REG_EXTENDED             - used for matching with substring table (translators)
*
*   PCRE_EXTENDED    - see PCRE_ flag definitions
*   PCRE_MULTILINE   - ^ and $ match on new line characters
*   PCRE_NEWLINE_ANY - $ matches CR LF CRLF
*/
struct REGEXP *newRegexp(char *expression,int flags){
  struct STRING *string;
  struct REGEXP *re,**reP;   
  pcre *preg;
  const char *msg=NULL;
  int offset;
  //int parens;

  string=useString(expression);
  reP=hashRegexp(regexpH,string,flags);  /* do we already have this regexp? */
  for(re=*reP;re!=NULL && (re->flags<flags || (re->flags==flags && re->string<string));re=*reP)
    reP=(struct REGEXP **)&re->object.next;  
  if(re!=NULL && re->flags==flags && re->string==string) return(re); // reuse if found
  preg=pcre_compile(expression,flags,&msg,&offset,NULL);
  if(preg==NULL){
    outMsg(0,'E',"Regular expression syntax error at: %s",expression);
    if(msg) outMsg(0,'E',"Regular expression syntax error at offset %d: %s",offset,msg);
    return(NULL);
    }
  re=newObject(regexpType,(void **)&freeRegexp,sizeof(struct REGEXP));
  re->string=grabObject(string);
  re->flags=flags;
  re->re=preg;
  re->object.next=(NB_Object *)*reP;
  re->pe=pcre_study(preg,0,&msg);
  re->nsub=0;
  if(pcre_fullinfo(preg,re->pe,PCRE_INFO_CAPTURECOUNT,&re->nsub)<0){
    dropObject(re->string);
    return(NULL);
    }
  *reP=re;
  return(re);
  }

/*
*  Print a regular expression
*/
void printRegexp(struct REGEXP *regexp){
  printObject((NB_Object *)regexp->string);
  }

/*
*  Regular expression destructor
*/
void destroyRegexp(struct REGEXP *regexp){
  struct REGEXP *re,**reP;

  reP=hashRegexp(regexpH,regexp->string,regexp->flags);  /* find it */
  for(re=*reP;re!=NULL && re->string<regexp->string;re=*reP)
    reP=(struct REGEXP **)&re->object.next;
  /* if yes, use it */   
  if(re==NULL || re->string!=regexp->string){
    outMsg(0,'L',"destroyRegexp() unable to locate expression.");
    return;
    }
  dropObject(regexp->string);
  /* check man page to see how we free the regular expression */
  *reP=(struct REGEXP *)regexp->object.next;
  regexp->object.next=(NB_Object *)freeRegexp;
  freeRegexp=regexp;
  }

/*
*  Context object type initialization
*/
void initRegexp(NB_Stem *stem){
  regexpH=newHash(137); /* initialize regular expression hash */
  regexpType=newType(stem,"~",regexpH,TYPE_REGEXP,printRegexp,destroyRegexp);
  }
