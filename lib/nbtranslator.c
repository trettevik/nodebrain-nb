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
* File:     nbtranslator.c
*
* Title:    Text Translator Object Manager
*
* Function:
*
*   This header provides routines to manage Nodebrain "text translator" 
*   objects used to translate character strings into Nodebrain commands
*   to be passed to the interpreter.
*
* Synopsis:
*
*   #include "nb.h"
*
*   nbCELL nbTranslatorCompile(nbCELL context,int reFlags,char *filename);
*
*   nbCELL nbTranslatorExecute(nbCELL context,nbCELL translator,char *string);
*
*   nbCELL nbTranslatorExecuteFile(nbCELL context,nbCELL translator,char *filename);
*
* Description:
*
*   A translator simply converts a character string in one "language" into
*   zero or more strings in another "language".  This is useful for recognizing
*   strings in a log file and translating them into nodebrain commands to
*   respond to the conditions reported in the log file.  It is also useful
*   for creating a pseudo macro language for a nodebrain application.
*
*   A nodebrain translator is based on standard regular expressions as
*   implemented by regcomp() and regexec() defined in "regex.h".  These
*   functions provide the logic to compile and execute extended regular 
*   expression.
*
*   We imbed regular expressions in a simple syntax (see below) used to define
*   what strings to emit when specific regular expressions are matched.  This
*   syntax allows for the specification of a tree structure, or list of lists.
*   There are four types of nodes in the tree (root,emit,match, and terminal).
*   We start from the root noded and move down the tree, stopping when we hit
*   a terminal node.  When an emit node is crossed we emit a character string.
*   When a match node is crossed we test to see if the input string matches
*   a regular expression.  If so, we take the right branch.  Otherwise, we    
*   take the left branch.
*
*                 R (root)
*                /
*               E (emit)
*              /
*             M (match)
*            / \
*           M   E
*          / \   \
*         M   T   M
*        /       / \
*       E       E   T (terminal)
*      /       /
*     T       T
*
*   
*   Within our syntax, the left branch takes us to the next statement at the
*   same level of nesting.  The right branch causes us to enter a new list
*   enclosed within braces {}. 
*
*   When a translator is loaded, load time symbolic substitution %{} is
*   supported.
*
*   When a string is emitted, emit-time symbolic substitution $[] is
*   supported.
*
*   When strings are passed into the nodebrain interpretter, context sensitive
*   symbolic substitution (${} $${} and $) is supported.
*   
*=============================================================================
* Syntax:
*
*   First non-space character of a line determines statement type
*
*     #  - comment
*
*          # any text can follow to end-of-line
*
*     <stmt> ::= [^] [<branch>] [ <edit> ] [<leaf>] 
*
*     <branch> ::= <branchNode> [?] [.<branch>]
*
*     <edit>   ::= <editOperators>
*
*     <leaf>   ::= <leafNode> | <statementBlock>
*
*   Branch Nodes:
*
*     "  - "string" statement (exact match)
*
*          "string_to_terminating_quote"
*
*     (  - "regex" statement (regular expression)
*
*          (regular_expression_to_balanced_paren)
*
*     [  - "projection" text [includes projection operators $[...])
*
*          [projection_string_to_balanced_bracket]
*
*   Statement Block:
*
*     {  - "block begin" 
*
*     }  - "block end" 
*
*   Leaf Nodes:
*
*     :  - emit statement (may include projection operators $[...])
*
*          : any NodeBrain command to end-of-line
*
*     $( - set return value to present value of cell expression
*
*          $(<cellExpression>)
*
*     $  - include statement (sources in another translator file)
*  
*          $filename
*
*     ~  - "link" statement (links to another translator file)
*
*          ~filename
*
*   Control Operators:
*     
*     ^     - verify branches exist up until next
*     ?     - show after update (follows branch)
*
*   Edit Operators:
*     
*     >[n]  - insert after statement n (after last if n not specified)
*     <[n]  - insert before statement n (before first if n not specified)
*     =[n]  - replace statement n (entire block if n not specified)
*     +!    - disabled branch
*     -!    - enable branch
*     +@    - set "and continue" option
*     -@    - unset "and continue" option
*     --    - delete  [not finished yet]
*
*   Projection Operators  (used in projection branch or emit statement)
*
*     $[0] - String matching outside parentheses (---)
*     $[1] - String matching first inner parentheses (...(---)...)
*     $[n] - String matching n'th inner parentheses
*
*     $[^] - Head string before $[0]
*     $[$] - Tail string after $[0]
*     $[-] - Full string for which a match was found
*              $[-] = $[^]$[0]$[$]
*     $[=] - Text we are currently trying to match
*              Set to $[$] by default
*              Set to different value using projection "[]" 
*
* Example:
*
*    ([a-d]*Fred):alert text="$[0]";
*    (dkdk kdkdfkdfkd fkdkkfkdfkd fkdkf dk){
*      : alert  
*      : assert x=1,y=2;
*      }
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-02-12 Ed Trettevik - original version 0.2.A
* 2003-01-28 eat - version 0.4.4
*            1) included ():statement syntax
*            2) included ":" as alternative to ">" for emit
*
* 2003-03-08 eat 0.5.1  Stopped passing trailing ) to regcomp by mistake.
*            This is treated like a regular character under some, but not all,
*            implementations.
*
* 2003-03-15 eat 0.5.1  Split into nbxtr.h and nbxtr.c for make file
* 2004-12-15 eat 0.6.0  Translator converted to NodeBrain object.
* 2005-04-04 eat 0.6.2  Modified to include in Skill Module API
* 2006-01-08 eat 0.6.4  Convert " to ' when doing symbolic substitution
*            This is weird, but it allows safe substitution of the form "$[3]" 
*            Don't see a better way around this at the moment. The user should
*            not have to fuss with this as they would in a general purpose
*            language.  As long as they know what to expect, it should be fine.
* 2006-05-09 eat 0.6.6  Removed symbolic context from nbTranslatorExecute()
* 2006-10-28 eat 0.6.6  Fixed bug in nbTranslatorCompile() associated with one-line IF statements
*            Regular expressions followed by a single command on the same line
*            where not properly assigned type=1 when translator symbolic substitution
*            was specified.           
*
*              (<regexp>): <command_including_$[n]_references>
*
*            Thanks to Benoit Dolez for reporting this bug.
*
* 2007-06-26 eat 0.6.8  Provided a way to alter the quote substitution
*            $[n]   - replace " with ' as before because placing matching strings
*                     into quoted string assertions is the most frequent use.
*
*                     $[3] - replace " with ' in string 3
*
*            $[.n]  - use the string asis, without replacing double quotes.
*
*                     $[.0] - use string 0 as is without replacement
*
*            $[xyn] - replace x with y
*
*                     $['"5] - replace ' with " in string 5
*                     $[;:2] - replace ; with : in string 2
*
*            We are assuming pretty simple requirements here, where there is
*            only one delimiter we need to worry about.  Since the goal is
*            normally to vacuum information up into NodeBrain assertions this
*            should be sufficient because NodeBrain syntax is simple.  If
*            something more complicated is required, the lines should be
*            passed to a servant written in a favorite language.
*
* 2008-09-20 eat 0.7.2  Encoded substitution to improve performance
* 2008-09-20 eat 0.7.2  Included '=' operation to replace source text
* 2008-09-20 eat 0.7.2  Changed syntax for symbolic substitution
*            $[n]   - replace " with ' as before because placing matching strings
*                     into quoted string assertions is the most frequent use.
*
*                     $[3] - replace " with ' in string 3
*
*            $[n,..] - use the string asis, without replacing double quotes.
*
*                     $[0,.] - use string 0 as is without replacement
*
*            $[n,xy] - replace x with y
*
*                     $[5,'"] - replace ' with " in string 5
*                     $[2,;:] - replace ; with : in string 2
*
* 2008-09-20 eat 0.7.2  Changed the match index numbers
*
*            $[0] - Full string for which a match was found
*            $[1] - String matching outside parentheses (---)              was $[0] before
*            $[2] - String matching first inner parentheses (...(---)...)  was $[1] before
*
* 2008-09-21 eat 0.7.2  Included three new match references
*
*                   $[0] = $[^]$[1]$[$]
*            $[^] - Head string before $[1]
*            $[$] - Tail string after $[1]
* 
*            $[=] - Text we are currently trying to match
*                   Set to $[$] by default
*                   Set to different value using "=" line operator
*
* 2008-09-21 eat 0.7.2  Included "@(" operator to fall through a match block
*            
*            The "(" operator starts a regular expression that will halt the
*            search on a match after executing the child statements.  These
*            statements fall through to subsequent statements at the same level
*            when the match fails.  The "@(" operator behaves the same way when
*            the match fails, but continues on after a successful match also.
*
* 2008-10-04 eat 0.7.2  Removed local symbolic context support
*
*            The approach previously used for the source symbolic context
*            was flawed.  A better approach in the future will be to implement
*            the full functionality of the SOURCE (%INCLUDE) command as a
*            preprocessor for translator files.  The "." operator should be 
*            enhanced at that time to support symbolic assertions.  It will
*            not be necessary to support symbolic assertions in the configuration
*            of nodes like translators, since they can reference unique files
*            that source a common file with variables as needed.
*
* 2008-10-07 eat 0.7.2  Changed the match index numbers back for compatibility
*
* 2008-10-14 eat 0.7.2  Converted projection into a NodeBrain object  
*
* 2008-10-14 eat 0.7.2  Included translator manipulation operators (see syntax above)
*
* 2008-10-14 eat 0.7.2  Replace '.' with '~' as translator file reference
*
* 2008-10-14 eat 0.7.2  Allow '.' as branch separator 
* 2008-11-06 eat 0.7.3  Converted to PCRE's native API
* 2008-11-11 eat 0.7.3  Change failure exit code to NB_EXITCODE_FAIL
* 2010-02-25 eat 0.7.9  cleaned up to remove -Wall warning messages
* 2010-02-26 eat 0.7.9  cleaned up to remove -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  cleaned up to remove -Wall warning messages (gcc 4.5.0)
* 2011-02-10 eat 0.8.5  turned echo off on call to nbCmd
* 2011-10-15 eat 0.8.6  modified nbTranslatorExecute to return a cell value
*                       
*            This enables the use of a translator for classification.  For
*            example, URL strings might be classified to determine the level
*            of authority required for access at a web site.
*
*            We've included "$" as a valid first character of a translator
*            line for this purpose.  It must be followed immediately by a
*            cell expression enclosed in parentheses, "$(<expression>)", which
*            is consistent with the "value of" syntax in NodeBrain cell
*            expressions.
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
* 2013-03-16 eat 0.8.15 Fixed defect in search for named regular expressions
* 2014-01-13 eat 0.9.00 Removed hash pointer - referenced via type
*=============================================================================
*/
#include <nb/nbi.h>

#define PROJECTION_STOP   0   // character used to terminate encoded projection 
#define PROJECTION_PARENS 32  // maximum number of parenthesized expressions
#define PROJECTION_INDEX  252-PROJECTION_PARENS // paren index
#define PROJECTION_HEAD   252 // String before match  $[^] as if ^(.*)...
#define PROJECTION_TAIL   253 // String after match   $[$] as if ...(.*)$
#define PROJECTION_TEXT   254 // Current text $[=] defaults to $[$] and set with =
#define PROJECTION_FULL   255 // Full text of last match $[-]
#define PROJECTION_SEGMENT PROJECTION_INDEX-1  // max literal segment size   

NB_Type       *nb_TranslatorType;
NB_Translator *nb_TranslatorFree=NULL;
NB_Translator *nb_Translators=NULL;

NB_Type       *nb_ProjectionType;
NB_Projection *nb_ProjectionFree=NULL;
//struct HASH   *nb_ProjectionHash=NULL;

#define NB_TRANSLATOR_STACKSIZE 64  // Parse and execution stack size

struct REGEXP_STACK{
  int count;   // number of entries in the stack
  struct REGEXP *regexp[NB_TRANSLATOR_STACKSIZE];
  };

/*===============================================================
* Projection Object Functions
*/

/*
*  Show a projection with names
*
*    If reStackP is not NULL, we attempt to display projection indexes
*    by name.  If we can't find a name for the index, we just display
*    the index number, which is equally valid syntax.
*/
void nbProjectionShowWithNames(NB_Projection *projection,struct REGEXP_STACK *reStackP){
  unsigned char *cursor;
  char text[NB_BUFSIZE],*textcur=text;
  int index,expDec,reIndex,foundname,namecount,namesize;
  unsigned char exp,charF,charT,*nameentry;
  pcre *re;
  pcre_extra *pe;

  if(projection->object.type!=nb_ProjectionType){ // handle string as alternative
    outPut("%s",((NB_String *)projection)->value);
    return;
    }
  cursor=(unsigned char *)projection->code;
  while(*cursor!=PROJECTION_STOP){
    if(*cursor<PROJECTION_INDEX){
      strncpy(textcur,(char *)cursor+1,*cursor);
      textcur+=*cursor;
      cursor+=*cursor+1;
      }
    else{
      strcpy(textcur,"$[");
      textcur+=2;
      index=*cursor,cursor++;
      exp=*cursor,cursor++;
      expDec=exp;
      charF=*cursor,cursor++;
      charT=*cursor,cursor++;
      while(exp>1) *textcur='.',textcur++,exp--;
      switch(index){
        case PROJECTION_HEAD: *textcur='<'; *(++textcur)=0; break;
        case PROJECTION_TAIL: *textcur='>'; *(++textcur)=0; break;
        case PROJECTION_TEXT: *textcur='='; *(++textcur)=0; break;
        case PROJECTION_FULL: *textcur='-'; *(++textcur)=0; break;
        case PROJECTION_INDEX: *textcur='~'; *(++textcur)=0; break;
        default:
          index-=PROJECTION_INDEX;
          foundname=0;
          if(reStackP!=NULL && (reIndex=reStackP->count-expDec)>=0){ 
            re=reStackP->regexp[reIndex]->re;
	    pe=reStackP->regexp[reIndex]->pe;
            if(pcre_fullinfo(re,pe,PCRE_INFO_NAMECOUNT,&namecount)==0){
              if(pcre_fullinfo(re,pe,PCRE_INFO_NAMEENTRYSIZE,&namesize)==0){
                if(pcre_fullinfo(re,pe,PCRE_INFO_NAMETABLE,&nameentry)==0){
                  while(namecount>0 && (*nameentry!=0 || *(nameentry+1)!=index)){
                    nameentry+=namesize;
                    namecount--;  
                    }
                  if(namecount>0){
                    sprintf(textcur,"%s",nameentry+2);
                    foundname=1;
                    }
                  }
                }
              }
            }
          if(!foundname) sprintf(textcur,"%d",index);
          textcur=textcur+strlen(textcur); // 2013-01-01 eat - VID 690-0.8.13-1 FP but changed from textcur=strchr(textcur,0);
        }
      if((charF!='"' || charT!='\'') && charF!=charT){
        *textcur=',',textcur++;
        *textcur=charF,textcur++;
        *textcur=charT,textcur++;
        } 
      *textcur=']',textcur++;
      }
    }
  *textcur=0;
  outPut("%s",text);
  }

/*
*  Show a projection object
*
*    Within a context where an reStack is being maintained, the
*    nbProjectionShowWithNames() function above can be used as an
*    alternative to display subexpression names instead of numbers.
*/
void nbProjectionShow(NB_Projection *projection){
  unsigned char *cursor;
  char text[NB_BUFSIZE],*textcur=text;
  int index;
  unsigned char exp,charF,charT;

  if(projection->object.type!=nb_ProjectionType){ // handle string as alternative
    outPut("%s",((NB_String *)projection)->value);
    return;
    }
  cursor=(unsigned char *)projection->code;
  while(*cursor!=PROJECTION_STOP){
    if(*cursor<PROJECTION_INDEX){
      strncpy(textcur,(char *)cursor+1,*cursor);
      textcur+=*cursor;
      cursor+=*cursor+1;
      }
    else{
      strcpy(textcur,"$[");
      textcur+=2;
      index=*cursor,cursor++;
      exp=*cursor,cursor++;
      charF=*cursor,cursor++;
      charT=*cursor,cursor++;
      while(exp>1) *textcur='.',textcur++,exp--;
      switch(index){
        case PROJECTION_HEAD: *textcur='<'; *(++textcur)=0; break;
        case PROJECTION_TAIL: *textcur='>'; *(++textcur)=0; break;
        case PROJECTION_TEXT: *textcur='='; *(++textcur)=0; break;
        case PROJECTION_FULL: *textcur='-'; *(++textcur)=0; break;
        case PROJECTION_INDEX: *textcur='~'; *(++textcur)=0; break;
        default:
          index-=PROJECTION_INDEX;
          sprintf(textcur,"%d",index);
          textcur+=strlen(textcur); // 2013-01-01 eat - VID 780-0.8.13-1 FP but changed from textcur=strchr(textcur,0);
        }
      if((charF!='"' || charT!='\'') && charF!=charT){
        *textcur=',',textcur++;
        *textcur=charF,textcur++;
        *textcur=charT,textcur++;
        }
      *textcur=']',textcur++;
      }
    }
  *textcur=0;
  outPut("%s",text);
  }

void nbProjectionShowAll(void){
  NB_Object *object,**objectP;
  long v;
  int i,saveshowcount=showcount;

  showcount=1;
  outPut("Projection Table:\n");
  objectP=(NB_Object **)&(nb_ProjectionType->hash->vect);
  for(v=0;v<=nb_ProjectionType->hash->mask;v++){
    i=0;
    for(object=*objectP;object!=NULL;object=(NB_Object *)object->next){
      outPut("[%u,%d]",v,i);
      printObject(object);
      outPut("\n");
      i++;
      }
    objectP++;
    }
  showcount=saveshowcount;
  }

void nbProjectionDestroy(NB_Projection *projection){
  NB_Projection **projectionP;
  NB_Hash *hash=nb_ProjectionType->hash;

  //projectionP=(NB_Projection **)hashStr(nb_ProjectionHash,projection->code);
  projectionP=(NB_Projection **)&(hash->vect[projection->object.hashcode&hash->mask]);
  for(;*projectionP!=NULL && *projectionP!=projection;projectionP=(NB_Projection **)&((*projectionP)->object.next));
  if(*projectionP==NULL) outMsg(0,'L',"Destroying projection not on used list");
  else *projectionP=(NB_Projection *)projection->object.next;
  nbFree(projection,sizeof(NB_Projection)+projection->length);
  hash->objects--;
  }  

static char *nbProjectionEncodeLiteral(char *bufcur,char *cursor,uint32_t len){  // 2013-01-01 eat - VID 787-0.8.13-1 changed len to unsigned
  while(len>PROJECTION_SEGMENT){
    *bufcur=PROJECTION_SEGMENT;
    bufcur++;
    memcpy(bufcur,cursor,PROJECTION_SEGMENT);  // 2013-01-01 eat - VID 5134-0.8.13-1 FP changed from strncpy to memcpy to help checker
    bufcur+=PROJECTION_SEGMENT;
    cursor+=PROJECTION_SEGMENT;
    len-=PROJECTION_SEGMENT;
    }
  *bufcur=len;  // 2013-01-01 eat - VID 663-0.8.13-1 FP len<=219 (PROJECTION_SEGMENT) - This is not numeric truncation
  bufcur++;
  strncpy(bufcur,cursor,len);  // 2013-01-01 - VID 5482-0.8.13-1 caller verifies bufcur is large enough
  bufcur+=len;
  return(bufcur);
  }
/*
*  Parse translator substitution
*
*  Input
*    ...$[n2]...$[n2]...
*  Output
*    n...text...  where n is length of text when n<=PROJECTION_SEGMENT
*    nfr          where n is pattern index, f is find char and r is replacement
*
*/
int nbProjectionEncode(char *buffer,int buflen,struct REGEXP_STACK *reStackP,int nsub[],int level,char *source){
  char *subcur,*bufcur=buffer,*cursor=source,*cursave;
  char exp,find,replace;
  size_t len;
  int i,reIndex=reStackP->count-1,initialReIndex=reIndex;
  char symid,ident[512];

  //outMsg(0,'T',"Encoding projection: %s",source);
  subcur=strstr(cursor,"$[");
  while(subcur!=NULL){
    reIndex=reStackP->count-1;
    len=subcur-cursor;
    //if(bufcur-buffer+len+((len/PROJECTION_SEGMENT)*3)>=buflen){
    if(bufcur-buffer+len>=buflen){
      outMsg(0,'E',"Line too large for encoded buffer size.");
      return(-1);
      }
    if(len>0) bufcur=nbProjectionEncodeLiteral(bufcur,cursor,len);
    subcur+=2;
    cursave=subcur;
    exp=0;   // start with 1 to avoid null termination when we use a string hash on the code
    while(*subcur=='.') exp++,reIndex--,subcur++;
    if(reIndex<0){
      outMsg(0,'E',"More periods than regular expressions in context at: %s",cursave);
      return(-1);
      }
    switch(*subcur){
      case '<': *bufcur=PROJECTION_HEAD; subcur++; break;
      case '>': *bufcur=PROJECTION_TAIL; subcur++; break;
      case '~': *bufcur=PROJECTION_INDEX; subcur++; break;
      case '-': *bufcur=PROJECTION_FULL; subcur++; break;
      case '=': // back-referenced TEXT is same as one less reference to FULL
        if(exp) *bufcur=PROJECTION_FULL,exp--;
        else    *bufcur=PROJECTION_TEXT;
        subcur++;
        break;
      default:
        cursave=subcur;
        symid=nbParseSymbol(ident,sizeof(ident),&subcur);
        if(symid=='i'){
          i=atoi(ident);
          if(i>reStackP->regexp[reIndex]->nsub){
            outMsg(0,'E',"Pattern index %d out of bounds for regular expression with %d sub-expressions.",i,reStackP->regexp[reIndex]->nsub);
            return(-1);
            }
          }
        else if(symid=='t'){
          i=pcre_get_stringnumber(reStackP->regexp[reIndex]->re,ident);
          while(i<0 && reIndex>0){ // 2013-03-16 eat - fixed defect of reIndex>=0
            exp++,reIndex--;
            i=pcre_get_stringnumber(reStackP->regexp[reIndex]->re,ident);
            }
          if(i<0){
            outMsg(0,'E',"Pattern name '%s' not defined in scope of regex: %s",ident,reStackP->regexp[initialReIndex]->string->value);
            return(-1);
            }
          }
        else{
          outMsg(0,'E',"Expecting pattern name or index at: %s",cursave);
          return(-1);
          }
        *bufcur=i+PROJECTION_INDEX;
      }
    bufcur++;
    find='"';     // replace double quote with single quote by default
    replace='\'';
    if(*subcur==','){
      subcur++;
      find=*subcur;                             // replace something specified
      subcur++;
      replace=*subcur;
      subcur++;
      if(replace==find) find=0;               // don't replace with same
      }
    if(*subcur==']') subcur++;
    else{
      outMsg(0,'E',"Pattern index error - expecting ']' at: %s",subcur);
      return(-1);
      }
    *bufcur=exp+1; bufcur++;
    *bufcur=find; bufcur++;
    *bufcur=replace; bufcur++;
    cursor=subcur;
    subcur=strstr(cursor,"$[");
    }
  len=strlen(cursor);
  if(len>0){
    //if(bufcur-buffer+len+((len/PROJECTION_SEGMENT)*3)>=buflen){
    if(bufcur-buffer+len>=buflen){
      outMsg(0,'E',"Line too large for encoded buffer size.");
      return(-1);
      }
    bufcur=nbProjectionEncodeLiteral(bufcur,cursor,len);
    }
  *bufcur=PROJECTION_STOP;
  bufcur++;
  return(bufcur-buffer);
  }


/*
*  Parse Projection
*
*    We return a projection or a string.  Code that uses projection objects
*    must test the object type and treat appropriately.
*/
NB_Projection *nbProjectionParse(char *projectionBuffer,int len,struct REGEXP_STACK *reStackP,int nsub[],int level,char *cursor){
  NB_Projection *projection,**projectionP;
  int plen,match=1;
  NB_Hash *hash=nb_ProjectionType->hash;
  uint32_t hashcode=0;

  if(strstr(cursor,"$[")==NULL) return((NB_Projection *)useString(cursor));
  plen=nbProjectionEncode(projectionBuffer,len,reStackP,nsub,level,cursor);
  if(plen<=0) return(NULL);
  //projectionP=(NB_Projection **)hashStr(nb_ProjectionHash,projectionBuffer);
  NB_HASH_STR(hashcode,projectionBuffer)
  projectionP=(NB_Projection **)&(hash->vect[hashcode&hash->mask]);
  for(;*projectionP!=NULL && (match=strcmp(projectionBuffer,(*projectionP)->code))>0;projectionP=(NB_Projection **)&((*projectionP)->object.next));
  if(match==0) projection=*projectionP;  // reuse if we find it
  else{
    projection=newObject(nb_ProjectionType,NULL,sizeof(NB_Translator)+plen);
    projection->object.hashcode=hashcode;
    projection->object.next=(NB_Object *)*projectionP;
    memcpy(&projection->code,projectionBuffer,plen);
    *projectionP=projection;
    hash->objects++;
    if(hash->objects>=hash->limit) nbHashGrow(&nb_ProjectionType->hash);
    }
  return(projection);
  }

//=================================================================

void nbTranslatorShowList(struct NB_XI *xi,int level,struct REGEXP_STACK *reStackP);
  
void nbTranslatorShowInstruction(struct NB_XI *xi,int level,struct REGEXP_STACK *reStackP){
  int i,reCount;
  if(xi->oper&NB_XI_OPER_DISABLED) outPut("!");
  if(xi->flag&NB_XI_FLAG_MATCHTHRU) outPut("@");
  switch(xi->oper&NB_XI_OPER_STATIC){
    case NB_XI_OPER_FILE:
      outPut("~%s\n",xi->item.label->value);
      break;
    case NB_XI_OPER_LABEL:
      outPut("%s",xi->item.label->value);
      if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
      else outPut("\n");
      break;
    case NB_XI_OPER_STRING:
      outPut("\"%s\"",xi->item.label->value);
      if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
      else outPut("\n");
      break;
    case NB_XI_OPER_REGEX:
      outPut("(%s)",xi->item.re->string->value);
      reCount=reStackP->count;
      reStackP->regexp[reCount]=xi->item.re;
      reStackP->count++;
      if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
      else outPut("\n");
      reStackP->count=reCount;
      break;
    case NB_XI_OPER_COMMAND:
      outPut(":");
      nbProjectionShowWithNames(xi->item.projection,reStackP);
      outPut("\n");
      break;
    case NB_XI_OPER_TRANSFORM:
      outPut("[");
      nbProjectionShowWithNames(xi->item.projection,reStackP);
      outPut("]");
      if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
      else outPut("\n");
      break;
    case NB_XI_OPER_SEARCH:
      xi=xi->nest;
      while(xi!=NULL){
        for(i=0;i<level;i++) outPut("  ");
        nbTranslatorShowInstruction(xi,level,reStackP);
        xi=xi->next;
        }
      break;
    case NB_XI_OPER_VALUE:
      outPut("$(");
      printObject((NB_Object *)xi->item.cell);
      outPut(")");
      if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
      else outPut("\n");
      break;
    // 2012-12-27 eat - CIDE 751542 - dead code commented out - was intended a protection against changes
    //default: 
    //  outPut("?OPER?");
    //  if(xi->nest) nbTranslatorShowList(xi->nest,level+1,reStackP);
    }
  }

void nbTranslatorShowList(struct NB_XI *xi,int level,struct REGEXP_STACK *reStackP){
  int i,n=0,oper;
  if(xi==NULL) return;
  if(xi->next==NULL){
    oper=xi->oper&NB_XI_OPER_STATIC;
    if(oper!=NB_XI_OPER_COMMAND && oper!=NB_XI_OPER_FILE && oper!=NB_XI_OPER_SEARCH) outPut(".");
    if(xi->oper!=NB_XI_OPER_SEARCH){
      nbTranslatorShowInstruction(xi,level-1,reStackP);
      return;
      }
    }
  if(level>1) outPut("{\n");
  while(xi!=NULL){
    if(xi->oper!=NB_XI_OPER_SEARCH){
      for(i=0;i<level;i++) outPut("  ");
      if((xi->oper&NB_XI_OPER_STATIC)==NB_XI_OPER_COMMAND || (xi->oper&NB_XI_OPER_STATIC)==NB_XI_OPER_FILE){
        n++;
        outPut("%d ",n);
        }
      }
    nbTranslatorShowInstruction(xi,level,reStackP);
    xi=xi->next;
    }
  if(level>1){
    for(i=0;i<level;i++) outPut("  ");
    outPut("}\n");
    }
  }

void nbTranslatorShow(nbCELL translatorCell){
  NB_Translator *translator=(NB_Translator *)translatorCell;
  struct REGEXP_STACK reStack;

  reStack.count=0;
  outPut("  Source: %s\n",translator->filename->value);
  if(translator->xi && translator->xi->nest) nbTranslatorShowList(translator->xi->nest,1,&reStack);
  }

/*
*  Free translator command tree
*
*  A file node points to another translator, so we do not free the subordinate branch
*/

void nbTranslatorFreeList(struct NB_XI *xi){
  struct NB_XI *xiNext;
  while(xi!=NULL){
    if(xi->nest!=NULL && (xi->oper&NB_XI_OPER_STATIC)!=NB_XI_OPER_FILE) nbTranslatorFreeList(xi->nest);
    if(xi->item.cell!=NULL) xi->item.cell=dropObject(xi->item.cell);
    xiNext=xi->next;
    nbFree(xi,sizeof(struct NB_XI));
    xi=xiNext;
    }
  }

void nbTranslatorDestroy(NB_Translator *translator){
  NB_Translator **translatorP;
  struct NB_XI *xiFile;

  if(translator->filename!=NULL){
    for(translatorP=&nb_Translators;*translatorP!=NULL && translator->filename<(*translatorP)->filename;translatorP=(NB_Translator **)&((*translatorP)->object.next));
    if(*translatorP && translator->filename==(*translatorP)->filename){ // 2012-12-27 eat 0.8.13 - CID 751551
      *translatorP=(NB_Translator *)(*translatorP)->object.next;
      }
    translator->filename=dropObject(translator->filename);
    }
  if(translator->xi!=NULL){
    xiFile=translator->xi;   // NB_XI_OPER_FILE entry
    if(xiFile->nest!=NULL) nbTranslatorFreeList(xiFile->nest);
    if(xiFile->item.cell!=NULL) xiFile->item.cell=dropObject(xiFile->item.cell); 
    nbFree(xiFile,sizeof(struct NB_XI));
    }
  nbFree(translator,sizeof(NB_Translator));
  }

/*
*  Initialize translator object type
*/
void nbTranslatorInit(NB_Stem *stem){
  nb_TranslatorType=newType(stem,"translator",NULL,0,nbTranslatorShow,nbTranslatorDestroy);
  nb_ProjectionType=newType(stem,"projection",NULL,0,nbProjectionShow,nbProjectionDestroy);
  }

/*
*  Execute translator command list
*
*    This algorithm uses two stacks
*
*      1) regex capture stack  - supports projection of captured strings at any level
*      2) instruction stack    - supports return to FAILTHRU and MATCHTHRU instruction nodes
*   
*/

struct NB_XC_STACK_ENTRY{
  char *text;                       // text to match against
  char *textbuf;                    // buffer where we can create new text
  size_t nmatch;                    // number of matches possible
  int ovector[PROJECTION_PARENS*3]; // pcre offset vector
  };

struct NB_XI_STACK_ENTRY{
  struct NB_XI *xi;
  struct NB_XC_STACK_ENTRY *xcStackP;
  struct NB_XI_STACK_ENTRY *matchThruP;
  };

nbCELL nbTranslatorExecute(NB_Cell *context,NB_Cell *translator,char *source){
  nbCELL value=NB_CELL_UNKNOWN;
  struct NB_XI    *xi,*xiNode;
  char *cursor,*found;
  int i,len;
  //int flag;
  struct NB_XC_STACK_ENTRY xcStack[NB_TRANSLATOR_STACKSIZE];     // string capture stack area
  struct NB_XC_STACK_ENTRY *xcStackP=xcStack,*xcStackRefP;
  struct NB_XI_STACK_ENTRY xiStack[NB_TRANSLATOR_STACKSIZE];   // instruction stack
  struct NB_XI_STACK_ENTRY *xiStackP=xiStack;
  char *text=source,textarea[32*1024],*textbuf=textarea; // text working area
  char *textend=textarea+sizeof(textarea);
  char *textcur;
  nbCELL stringCell;
  NB_TreePath treePath;
  int backref,nmatch;

  // We should add an option to display source lines here
  //outPut("] %s\n",source);
  //outMsg(0,'T',"Translator depth=%d",((NB_Translator *)translator)->depth);

  // Initialize the regex string capture stack as if we just matched on the complete string

  xcStackP->text=source;
  xcStackP->textbuf=textbuf;
  xcStackP->nmatch=0;
  xcStackP->ovector[0]=0;
  xcStackP->ovector[1]=strlen(source);

  // Initialize the instruction stack

  xiStackP=xiStack;
  xiStackP->xi=NULL;
  xiStackP->xcStackP=xcStack;
  xiStackP->matchThruP=xiStackP;

  // Execute translation algorithm
  for(xi=((NB_Translator *)translator)->xi->nest;xi!=NULL;){
    //outMsg(0,'T',"oper=%u flag=%u xcStack=%u",xi->oper,xi->flag,xcStackP);
    switch(xi->oper){
      case NB_XI_OPER_FILE:
      case NB_XI_OPER_LABEL:
        xiStackP++;
        xiStackP->xi=xi;
        xiStackP->xcStackP=xcStackP;
        if(xi->flag&NB_XI_FLAG_MATCHTHRU) xiStackP->matchThruP=xiStackP;
        else xiStackP->matchThruP=(xiStackP-1)->matchThruP;
        xi=xi->nest;
        break;
      case NB_XI_OPER_REGEX:
        nmatch=xi->item.re->nsub+1;
        xcStackP++;  // push the capture stack
        //outMsg(0,'T',"matching text: %s",text);
        if(pcre_exec(xi->item.re->re,xi->item.re->pe,text,strlen(text),0,0,xcStackP->ovector,nmatch*3)>0){
          //outMsg(0,'T',"match found");
          if(xi->flag&NB_XI_FLAG_MATCHTHRU){
            //outMsg(0,'T',"matchthru flag found");
            xiStackP++;
            xiStackP->xi=xi;
            xiStackP->xcStackP=xcStackP;
            xiStackP->matchThruP=xiStackP;
            }
          else xiStackP=xiStackP->matchThruP;
          xcStackP->nmatch=nmatch;
          xcStackP->text=text;            // keep track of text we used for this match
          xcStackP->textbuf=textbuf;      // keep track of buffer for generating new text
          text=text+xcStackP->ovector[1]; // match on rest of text by default
          xi=xi->nest;
          if(!xi) value=NB_CELL_TRUE;
          }
        else xcStackP--,xi=xi->next;
        break;
      case NB_XI_OPER_SEARCH:
        stringCell=nbCellCreateString(context,text);
        xiNode=(struct NB_XI *)nbTreeLocate(&treePath,stringCell,(NB_TreeNode **)&(xi->tree));
        if(xiNode==NULL || xiNode->oper&NB_XI_OPER_DISABLED) xi=xi->next;
        else{
          if(xiNode->flag&NB_XI_FLAG_MATCHTHRU){
            xiStackP++;
            xiStackP->xi=xi;
            xiStackP->xcStackP=xcStackP;
            xiStackP->matchThruP=xiStackP;
            }
          else xiStackP=xiStackP->matchThruP;
          xi=xiNode->nest;
          if(!xi) value=NB_CELL_TRUE;
          }
        nbCellDrop(context,stringCell);
        break;
      case NB_XI_OPER_TRANSFORM:
        xiStackP++;
        xiStackP->xi=xi;
        xiStackP->xcStackP=xcStackP;
        if(xi->flag&NB_XI_FLAG_MATCHTHRU) xiStackP->matchThruP=xiStackP;
        else xiStackP->matchThruP=(xiStackP-1)->matchThruP;
        // fall thru to share projection logic with commands
      case NB_XI_OPER_COMMAND:
        if(xi->item.projection->object.type!=nb_ProjectionType){
          textcur=textbuf; 
          if(strlen(((NB_String *)xi->item.projection)->value)<textend-textbuf){
            strcpy(textbuf,((NB_String *)xi->item.projection)->value);
            textcur+=strlen(textbuf);
            } 
          else outMsg(0,'L',"Translator text area is full");
          }
        else{
          cursor=xi->item.projection->code;
          textcur=textbuf;
          while(*cursor!=PROJECTION_STOP){
            if((unsigned char)*cursor<PROJECTION_INDEX){
              strncpy(textcur,cursor+1,*cursor);
              textcur+=*cursor;
              cursor+=*cursor+1;
              }
            else{
              len=0;
              backref=*(cursor+1);         // get back reference
              backref--;                   // compensating for starting with 1
              xcStackRefP=xcStackP-backref;
              switch((unsigned char)*cursor){
                case PROJECTION_HEAD:
                  len=xcStackRefP->ovector[0];
                  strncpy(textcur,xcStackRefP->text,len);  
                  break;
                case PROJECTION_TAIL:
                  len=strlen(xcStackRefP->text+xcStackRefP->ovector[1]);
                  strncpy(textcur,xcStackRefP->text+xcStackRefP->ovector[1],len);
                  break;
                case PROJECTION_TEXT:
                  len=strlen(text);
                  memcpy(textcur,text,len); // 2013-01-01 eat - VID 5459-0.8.13-1 FP changed from strncpy to memcpy to help checker
                  break;
                case PROJECTION_FULL:
                  len=strlen(xcStackRefP->text);
                  memcpy(textcur,xcStackRefP->text,len); // 2013-01-01 eat - VID 4730 FP changed from strncpy to memcpy to help checker
                  break;
                default:
                  i=(unsigned char)*cursor;
                  i-=PROJECTION_INDEX;
                  len=xcStackRefP->ovector[i*2+1]-xcStackRefP->ovector[i*2];
                  strncpy(textcur,xcStackRefP->text+xcStackRefP->ovector[i*2],len);
                }
              if(len && *(cursor+2)){   // replace charF with charR if requested
                found=memchr(textcur,*(cursor+2),len);
                while(found!=NULL){
                  *found=*(cursor+3);
                  found=memchr(found,*(cursor+2),textcur+len-found);
                  }
                }
              textcur+=len;
              cursor+=4;
              }
            }
          *textcur=0;
          }
        if(xi->oper==NB_XI_OPER_COMMAND){
          if(*textbuf) nbCmd(context,textbuf,1);
          xi=xi->next;
          }
        else{  // transform
          text=textbuf;
          textbuf=textcur+1;
          xi=xi->nest;
          }
        break;
      case NB_XI_OPER_VALUE:  // assign value
        value=xi->item.cell;
        xi=xi->next;
        break;
      default:
        if(xi->oper&NB_XI_OPER_DISABLED) xi=xi->next;
        else nbExit("Unrecognized translator instruction operation=%d - terminating",xi->oper);
      }
    // see if we need to continue after processing a command list
    if(xi==NULL && (xi=xiStackP->xi)!=NULL){
      //outMsg(0,'T',"Trying to continue");
      xi=xi->next;
      xcStackP=xiStackP->xcStackP;
      text=xcStackP->text;  // 2009-01-28 eat 
      xiStackP--;
      }
    //else outMsg(0,'T',"Not trying to continue");
    }
  return(value);   // return last assigned value
  }

void nbTranslatorExecuteFile(nbCELL context,nbCELL translator,char *filename){
  FILE *file;
  char *cursor;
  char source[NB_BUFSIZE];

  //outMsg(0,'I',"Translating file \"%s\"",filename);
  if((file=fopen(filename,"r"))==NULL){                   // 2013-01-01 eat - VID 636-0.8.13-1 Intentional
    outMsg(0,'E',"Unable to open \"%s\"",filename);
    return;
    }
  outPut("---------- --------> %s\n",filename);
  //outBar();
  while(fgets(source,NB_BUFSIZE-1,file)){
    cursor=source+strlen(source);
    cursor--;
    while(cursor>=source && (*cursor==10 || *cursor==13)){
      *cursor=0;
      cursor--;
      }
    nbTranslatorExecute(context,translator,source);
    }
  fclose(file);
  //outBar();
  outPut("---------- --------< %s\n",filename);
  }

/********************************************************
* Parsing functions
*/

nbCELL nbTranslatorParse(nbCELL context,char *filename,int nsub);

static int nbTranslatorParseNumber(char **cursorP){
  char number[10],*curnum=number,*cursor=*cursorP;
  
  while(*cursor>='0' && *cursor<='9' && curnum<number+9){
    *curnum=*cursor;
    curnum++;
    cursor++; 
    }
  if(curnum>=number+9){
    outMsg(0,'E',"Number is too large at: %s",*cursorP);
    return(-1);
    }
  *curnum=0;
  *cursorP=cursor;
  return(atoi(number));
  }

/*
*  Insert positional command list (COMMAND, FILE, or LIST)
*/
static int nbTranslatorInsertList(struct NB_XI *xi,struct NB_XI *xiParent,unsigned char flag,int position){
  struct NB_XI **xiP,*xiLast,*xiTarget;

  if(xi==NULL){
     if(xiParent->nest!=NULL){
       nbTranslatorFreeList(xiParent->nest);
       xiParent->nest=NULL;
       }
     return(1);
     }
  for(xiLast=xi;xiLast->next!=NULL;xiLast=xiLast->next);
  xiP=&xiParent->nest;
  if(position){
    while(position && *xiP!=NULL){
      xiTarget=*xiP;
      if((xiTarget->oper&NB_XI_OPER_STATIC)==NB_XI_OPER_COMMAND || (xiTarget->oper&NB_XI_OPER_STATIC)==NB_XI_OPER_FILE) position--;
      if(position) xiP=&(xiTarget->next);
      }
    if(*xiP==NULL){
      outMsg(0,'E',"Statement number not found.");
      return(-1);
      }
    position=1;  // flag we are positioning
    }
  if(flag&NB_XI_FLAG_BEFORE){
    xiLast->next=*xiP;
    }
  else if(flag&NB_XI_FLAG_REPLACE){
    if(position){
      xiLast->next=(*xiP)->next;
      (*xiP)->next=NULL;   // clear old statement's next pointer before FreeList call
      }
    nbTranslatorFreeList(*xiP);
    }
  else{
    if(position){
      xiP=&((*xiP)->next);
      xiLast->next=*xiP;
      }
    else while(*xiP!=NULL) xiP=&((*xiP)->next); 
    }
  *xiP=xi;
  return(1);
  }

static int nbTranslatorDeleteLeaf(struct NB_XI *xiParent,int position){
  struct NB_XI *xi,**xiP;
  int n=0,oper;

  for(xiP=&xiParent->nest;*xiP!=NULL;xiP=&(*xiP)->next){
    xi=*xiP;
    oper=xi->oper&NB_XI_OPER_STATIC;
    if(oper==NB_XI_OPER_COMMAND || oper==NB_XI_OPER_FILE){
      n++;
      if(n==position){
        *xiP=xi->next;  // point over leaf removed
        xi->next=NULL;
        nbTranslatorFreeList(xi);
        return(1);
        }
      }
    }
  outMsg(0,'E',"Leaf node %d not found.",position);
  return(0);
  }

static int nbTranslatorDeleteBranch(struct NB_XI *xiParent,NB_Object *object){
  NB_TreePath treePath;
  struct NB_XI *xi,**xiP;

  outMsg(0,'T',"nbTranslatorDeleteBranch: called");
  for(xiP=&xiParent->nest;*xiP!=NULL && (NB_Object *)((*xiP)->item.cell)!=object;xiP=&(*xiP)->next);
  if(*xiP==NULL){
    outMsg(0,'E',"Branch not found.");
    return(0);
    }
  xi=(struct NB_XI *)nbTreeLocate(&treePath,object,(NB_TreeNode **)&(xiParent->tree));
  if(xi==NULL){
    outMsg(0,'E',"Branch not found.");
    return(0);
    }
  if(xi!=*xiP){
    outMsg(0,'L',"nbTranslatorDeleteBranch: Corrupted translator tree");
    return(0);
    }
  nbTreeRemove(&treePath);
  *xiP=xi->next;
  xi->next=NULL;
  nbTranslatorFreeList(xi);
  return(1);
  }

static struct NB_XI *nbTranslatorInsertBranch(struct NB_XI *xiParent,nbCELL object,unsigned char oper,unsigned char flag,int position,char *cursor){
  NB_TreePath treePath;
  struct NB_XI *xi,**xiP;

  xi=(struct NB_XI *)nbTreeLocate(&treePath,object,(NB_TreeNode **)&(xiParent->tree));
  if(xi==NULL){
    if(flag&NB_XI_FLAG_REUSE){
      outMsg(0,'E',"Branch not found at: %s",cursor);
      return(NULL);
      }
    xi=nbAlloc(sizeof(struct NB_XI));
    memset(xi,0,sizeof(struct NB_XI));
    xi->item.cell=grabObject(object);
    xi->oper=oper;
    xi->flag=flag;
    nbTreeInsert(&treePath,(NB_TreeNode *)xi);
    // For string branches we default to BEFORE for performance because order has no other impact
    if(flag&NB_XI_FLAG_BEFORE || (!(flag&NB_XI_FLAG_MODIFY) && (xiParent->oper&NB_XI_OPER_STATIC)==NB_XI_OPER_SEARCH)){
      xi->next=xiParent->nest;
      xiParent->nest=xi;
      }
    else if(flag&NB_XI_FLAG_REPLACE){
      nbTranslatorFreeList(xiParent->nest);
      xiParent->nest=xi;
      }
    else{
      for(xiP=&(xiParent->nest);*xiP!=NULL;xiP=&((*xiP)->next));
      *xiP=xi;
      }
    }
  else if(flag&NB_XI_FLAG_NEW){
    outMsg(0,'E',"Branch already defined at: %s",cursor);
    return(NULL);
    }
  else if((flag&NB_XI_FLAG_MATCHTHRU)!=(xi->flag&NB_XI_FLAG_MATCHTHRU)){
    outMsg(0,'E',"Continue option @ inconsistency at: %s",cursor);
    return(NULL);
    }
  return(xi);
  }

static int nbTranslatorInclude(nbCELL context,char *filename,struct REGEXP_STACK *reStackP,int nsub[],int level,int *depthP,struct NB_XI *xiParent,int reFlags);

/*
*  Parse translator statement
*
*  Returns:
*
*     0 - syntax error
*     1 - valid statement
*
*  Note: source must be NB_BUFSIZE bytes
*/
static int nbTranslatorParseStmt(nbCELL context,char *cursor,char *source,FILE *file,struct REGEXP_STACK *reStackP,int nsub[],int level,int *depthP,struct NB_XI *xiParent,unsigned char flag,int reFlags){
  //char symid;
  char ident[512];
  //struct NB_XI **xiP;
  struct NB_XI *xi,*xiNode;
  int rc,paren;
  char projectionBuffer[NB_BUFSIZE];
  char *delim,*curthen,*cursave;
  //char msg[1024];
  NB_Translator *translator;
  //NB_TreePath treePath;
  nbCELL stringCell;
  NB_Projection *projectionCell;
  struct REGEXP *regexCell;
  int depth=0;
  int position=0;
  int delete=0;
  int reCount;

  //outMsg(0,'T',"[parse] %s",cursor);
  //outMsg(0,'T',"level=%d nsub[level]=%d",level,nsub[level]); 
  //nsub[level]=nsub[level-1];

  flag&=NB_XI_FLAG_INHERIT;     // only keep flags that can be inheritted from left to right

  if(level>*depthP) *depthP=level;
  while(*cursor==' ' || *cursor=='\t') cursor++;
  if(*cursor=='^'){          // this should move up to the caller, because it must start the statement
    flag|=NB_XI_FLAG_REUSE;
    cursor++;
    while(*cursor==' ' || *cursor=='\t') cursor++;
    }
  if(*cursor=='.') cursor++;
  if(*cursor=='@'){
    flag|=NB_XI_FLAG_MATCHTHRU;
    cursor++;
    while(*cursor==' ' || *cursor=='\t') cursor++;
    }
  if(NB_ISNUMERIC((int)*cursor)){
    cursave=cursor;
    position=nbTranslatorParseNumber(&cursor);
    if(position<0){
      outMsg(0,'E',"Invalid position at: %s",cursave);
      return(0);
      }
    while(*cursor==' ' || *cursor=='\t') cursor++;
    }
  if(*cursor && strchr("|><=",*cursor)){
    flag&=NB_XI_FLAG_ALL^NB_XI_FLAG_REUSE;   // turn off REUSE flag
    switch(*cursor){
      case '|': delete=1; break;
      case '>': flag|=NB_XI_FLAG_AFTER;     break;
      case '<': flag|=NB_XI_FLAG_BEFORE;    break;
      case '=': flag|=NB_XI_FLAG_REPLACE;   break;
      default:
        outMsg(0,'L',"Option character '%c' not recognized",*cursor);
        return(0);
      }
    cursor++;
    while(*cursor==' ' || *cursor=='\t') cursor++;
    cursave=cursor;
    if(position>0 && !delete && *cursor!=':' && *cursor!='~' && *cursor!='$' && *cursor!='{'){
      outMsg(0,'E',"Number must be followed by '{', ':' or '.' at: %s",cursave);
      return(0);
      }
    while(*cursor==' ' || *cursor=='\t') cursor++;
    }
  //outMsg(0,'T',"[char] %d",*cursor);
  if(delete){
    if(position) return(nbTranslatorDeleteLeaf(xiParent,position));
    if(*cursor!='(' && *cursor!='[' && *cursor!='"' && !NB_ISALPHA((int)*cursor) && !NB_ISNUMERIC((int)*cursor)){
      outMsg(0,'E',"Expecting branch node identifier at: %s",cursor);
      return(0);
      }
    }
  switch(*cursor){
    case 0:
      if(flag&NB_XI_FLAG_REPLACE){
        if(!nbTranslatorInsertList(NULL,xiParent,flag,position)) return(0);
        //nbTranslatorFreeList(xiParent->nest);
        //xiParent->nest=NULL;
        }
      return(1);

    case '#':
      return(1);

    case '+':
      cursor++;
      if(*cursor=='@') xiParent->flag|=NB_XI_FLAG_MATCHTHRU;
      else if(*cursor=='!') xiParent->oper|=NB_XI_OPER_DISABLED;
      else{
        outMsg(0,'E',"Expecting @ or ! at: %s",cursor);
        return(0);
        }
      cursor++;
      while(*cursor==' ' || *cursor=='\t') cursor++;
      if(*cursor && *cursor!='#') outMsg(0,'W',"Expecting '#' or end of line at: %s",cursor);
      return(1);

    case '-':
      cursor++;
      if(*cursor=='@') xiParent->flag&=NB_XI_OPER_ALL^NB_XI_FLAG_MATCHTHRU;
      else if(*cursor=='!') xiParent->oper&=NB_XI_OPER_ALL^NB_XI_OPER_DISABLED;
      else{
        outMsg(0,'E',"Expecting @ or ! at: %s",cursor);
        return(0);
        }
      cursor++;
      while(*cursor==' ' || *cursor=='\t') cursor++;
      if(*cursor && *cursor!='#') outMsg(0,'W',"Expecting '#' or end of line at: %s",cursor);
      return(1);

    case '?':
      cursor++;
      rc=nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level,depthP,xiParent,flag,reFlags);
      outPut("-->\n  ");
      nbTranslatorShowInstruction(xiParent,1,reStackP);
      outPut("--<\n  ");
      return(rc);

    case '!':
      cursor++;
      xi=nbAlloc(sizeof(struct NB_XI));
      memset(xi,0,sizeof(struct NB_XI));
      xi->oper=NB_XI_OPER_LABEL;
      if(!nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level,&depth,xi,0,reFlags)){
        nbTranslatorFreeList(xi);
        return(0);
        }
      nbTranslatorFreeList(xi);
      return(1);

    case '{':
      if(!file){  // 2012-12-27 eat 0.8.13 - CID 751552
        outMsg(0,'E',"Symbol { only recognized within context of a file");
        return(0);
        }
      cursor++;
      while(*cursor==' ') cursor++;
      if(*cursor && *cursor!='#'){
        outMsg(0,'E',"Expecting '#' or end of line at: %s",cursor);
        return(0);
        }
      if(flag&(NB_XI_FLAG_BEFORE|NB_XI_FLAG_REPLACE)){
        xiNode=nbAlloc(sizeof(struct NB_XI));
        memset(xiNode,0,sizeof(struct NB_XI));
        xiNode->oper=NB_XI_OPER_LABEL;
        }
      else xiNode=xiParent;        // AFTER is default operation
      while(fgets(source,NB_BUFSIZE,file)){  // 2013-01-01 eat - VID 5481-0.8.13-1 FP we know the size of source is NB_BUFSIZE
        outPut("%s",source);
        cursor=source;
        while(*cursor==' ') cursor++;
        if(*cursor=='}'){
          if(xiNode!=xiParent){
            if(!nbTranslatorInsertList(xiNode->nest,xiParent,flag,position)) return(0);
            xiNode->nest=NULL;
            nbTranslatorFreeList(xiNode);
            }
          return(1);
          }
        delim=cursor+strlen(cursor); // 2013-01-01 eat - VID 4734-0.8.13-1 FP but changed from delim=strchr(cursor,0); to help checker
        delim--;
        if(*delim=='\n'){
          *delim=0;
          delim--;
          if(*delim=='\r') *delim=0;
          }
        while(*delim==' '){
          *delim=0;
          delim--;
          }
        if(!nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level,depthP,xiNode,flag,reFlags)) return(0);
        }
      outMsg(0,'E',"End of file reached before closing brace '}'");
      return(0);

    case '"':
      xiNode=xiParent->nest;
      if(xiNode==NULL || xiNode->oper!=NB_XI_OPER_SEARCH){
        xi=nbAlloc(sizeof(struct NB_XI));
        memset(xi,0,sizeof(struct NB_XI));
        xi->oper=NB_XI_OPER_SEARCH;
        xi->next=xiParent->nest;
        xiParent->nest=xi;
        xiNode=xi;
        }
      cursor++;
      delim=strchr(cursor,'"');
      if(!delim){
        outMsg(0,'E',"Unbalanced quote in string entry: %s",cursor);
        return(0);
        }
      *delim=0;
      stringCell=nbCellCreateString(context,cursor);
      xi=nbTranslatorInsertBranch(xiNode,(nbCELL)stringCell,NB_XI_OPER_STRING,flag,position,cursor);
      if(!xi) return(0);
      if(!nbTranslatorParseStmt(context,delim+1,source,file,reStackP,nsub,level,depthP,xi,flag,reFlags)) return(0);
      return(1);

    case '(':
      cursor++;
      paren=1;
      curthen=cursor;
      while(*curthen!=0 && paren>0){
        if(*curthen=='\\' && *(curthen+1)!=0) curthen++;
        else if(*curthen=='(') paren++;
        else if(*curthen==')') paren--;
        curthen++;
        }
      if(paren>0){
        outMsg(0,'E',"Unbalanced parentheses in match condition");
        return(0);
        }
      curthen--;
      *curthen=0; /* terminate regular expression */

      //regexCell=newRegexp(cursor,0);
      //regexCell=newRegexp(cursor,PCRE_MULTILINE);
      regexCell=newRegexp(cursor,reFlags);
      if(!regexCell) return(0);
      grabObject(regexCell);
      if(regexCell->nsub>PROJECTION_PARENS){
        outMsg(0,'E',"Only %d sub-expressions may be referenced.",PROJECTION_PARENS);
        dropObject(regexCell);
        return(0);
        }
      if(delete){
        if(!nbTranslatorDeleteBranch(xiParent,(NB_Object *)regexCell)) return(0);
        return(1);
        }
      xi=nbTranslatorInsertBranch(xiParent,(nbCELL)regexCell,NB_XI_OPER_REGEX,flag,position,cursor);
      if(!xi) return(0);
      nsub[level+1]=xi->item.re->nsub;
      reCount=reStackP->count;   // save regular expression count
      reStackP->regexp[reCount]=regexCell;
      reStackP->count++;         // increment regular expression count
      cursor=curthen+1;
      while(*cursor==' ') cursor++;
      if(!nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level+1,depthP,xi,flag,reFlags)) return(0);
      reStackP->count=reCount;   // restore regular expression count
      return(1);

    case '[':  // Text Projection
      cursor++;
      paren=1;
      curthen=cursor;
      while(*curthen!=0 && paren>0){
        if(*curthen=='\\' && *(curthen+1)!=0) curthen++;
        else if(*curthen=='[') paren++;
        else if(*curthen==']') paren--;
        curthen++;
        }
      if(paren>0){
        outMsg(0,'E',"Unbalanced brackets in projection");
        return(0);
        }
      curthen--;
      *curthen=0; /* terminate projection */
      projectionCell=nbProjectionParse(projectionBuffer,sizeof(projectionBuffer),reStackP,nsub,level,cursor);
      if(projectionCell==NULL) return(0);
      if(delete){
        if(!nbTranslatorDeleteBranch(xiParent,(NB_Object *)projectionCell)) return(0);
        return(1);
        }
      xi=nbTranslatorInsertBranch(xiParent,(nbCELL)projectionCell,NB_XI_OPER_TRANSFORM,flag|NB_XI_FLAG_FAILTHRU,position,cursor);
      if(xi==NULL) return(0);
      cursor=curthen+1;
      nsub[level+1]=nsub[level];
      if(!nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level+1,depthP,xi,0,reFlags)) return(0);
      if(xi->nest==NULL) outMsg(0,'W',"Projection branch has no content");
      return(1);

    case ':':  // Command Projection
      cursor++;
      //outMsg(0,'T',"Command level=%d nsub[level]=%d",level,nsub[level]);
      projectionCell=nbProjectionParse(projectionBuffer,sizeof(projectionBuffer),reStackP,nsub,level,cursor);
      if(projectionCell==NULL) return(0);
      xi=nbAlloc(sizeof(struct NB_XI));
      memset(xi,0,sizeof(struct NB_XI));
      xi->item.projection=grabObject(projectionCell);
      if(flag&NB_XI_FLAG_MATCHTHRU){
        outMsg(0,'W',"Continue operator '@' is implied for command statement.");
        flag^=NB_XI_FLAG_MATCHTHRU;
        }
      xi->oper=NB_XI_OPER_COMMAND;
      xi->flag|=flag;
      if(!nbTranslatorInsertList(xi,xiParent,flag,position)) return(0);
      return(1);

    case '$':  // translator file include or return value setting
      cursor++;
      if(*cursor=='('){  // return value setting
        cursor++;
        xi=nbAlloc(sizeof(struct NB_XI));
        memset(xi,0,sizeof(struct NB_XI));
        xi->item.cell=(nbCELL)nbParseCell((NB_Term *)context,&cursor,0);
        if(!xi->item.cell){
          outMsg(0,'E',"Syntax error at-->%s",*cursor);
          nbFree(xi,sizeof(struct NB_XI));
          return(0);
          }
        if(*cursor!=')'){
          outMsg(0,'E',"Unbalanced parentheses [%s].",*cursor);
          nbFree(xi,sizeof(struct NB_XI));
          return(0);
          }
        xi->item.cell=nbCellCompute(context,xi->item.cell);  // get the value of the cell
        if(flag&NB_XI_FLAG_MATCHTHRU){
          outMsg(0,'W',"Continue operator '@' is implied for command statement.");
          flag^=NB_XI_FLAG_MATCHTHRU;
          }
        xi->oper=NB_XI_OPER_VALUE;
        xi->flag|=flag;
        if(!nbTranslatorInsertList(xi,xiParent,flag,position)) return(0);
        outMsg(0,'T',"Inserted value operation");
        return(1);
        }
        
      // translator file include
      while(*cursor==' ') cursor++;    
      xiNode=nbAlloc(sizeof(struct NB_XI));
      memset(xiNode,0,sizeof(struct NB_XI));
      xiNode->oper=NB_XI_OPER_FILE;
      nsub[level+1]=nsub[level];
      if(!nbTranslatorInclude(context,cursor,reStackP,nsub,level+1,depthP,xiNode,reFlags)){
        nbTranslatorFreeList(xiNode);
        return(0);
        }
      xi=xiNode->nest;
      nbFree(xiNode,sizeof(struct NB_XI));
      if(!nbTranslatorInsertList(xi,xiParent,flag,position)) return(0);
      return(1);

    case '~':  // translator file reference
      cursor++;
      while(*cursor==' ') cursor++;
      //if((translator=(NB_Translator *)nbTranslatorParse(context,cursor,nsub))==NULL){
      if((translator=(NB_Translator *)nbTranslatorCompile(context,reFlags,cursor))==NULL){
        outMsg(0,'E',"Unable to load nested translator \"%s\"",cursor);
        return(0);
        }
      if(level+translator->depth>*depthP) (*depthP)+=translator->depth;
      // we clone the translator's FILE node so we can indepently disable and free it
      xi=nbAlloc(sizeof(struct NB_XI));
      memset(xi,0,sizeof(struct NB_XI));
      xi->item.cell=grabObject(translator->xi->item.cell);
      xi->oper=NB_XI_OPER_FILE;
      xi->flag=flag;
      xi->nest=translator->xi->nest;
      if(!nbTranslatorInsertList(xi,xiParent,flag,position)) return(0);
      return(1);

    default:
      cursave=cursor;
      if(NB_ISALPHA((int)*cursor)){
        curthen=ident;
        while(NB_ISALPHANUMERIC((int)*cursor) && curthen<ident+sizeof(ident)-1){
          *curthen=*cursor;
          curthen++;
          cursor++;
          }
        *curthen=0;
        stringCell=nbCellCreateString(context,ident);
        if(delete){
          if(!nbTranslatorDeleteBranch(xiParent,(NB_Object *)stringCell)) return(0);
          return(1);
          }
        xi=nbTranslatorInsertBranch(xiParent,(nbCELL)stringCell,NB_XI_OPER_LABEL,flag|NB_XI_FLAG_FAILTHRU,position,cursor);
        if(!xi) return(0);
        nsub[level+1]=nsub[level];
        if(!nbTranslatorParseStmt(context,cursor,source,file,reStackP,nsub,level+1,depthP,xi,flag,reFlags)) return(0);
        if(xi->nest==NULL) outMsg(0,'W',"Label branch has no content");
        return(1);
        }
    }
  // 2012-12-27 eat 0.8.13 - CID 751689 - moved out of default to replace previously dead code here
  outMsg(0,'E',"Expecting one of #!@[(:.{}^><=-+ or alpha as first non-space character at: %s",cursave);
  return(0);
  }


/*
*  Load translator command list
*/

static int nbTranslatorInclude(nbCELL context,char *filename,struct REGEXP_STACK *reStackP,int nsub[],int level,int *depthP,struct NB_XI *xiParent,int reFlags){
  char source[NB_BUFSIZE];
  FILE *file;
  char *delim;

  outMsg(0,'I',"Loading translator \"%s\"",filename);
  if((file=fopen(filename,"r"))==NULL){
    outMsg(0,'E',"Unable to open \"%s\"",filename);
    return(0);
    }
  outBar();
  while(fgets(source,NB_BUFSIZE,file)){
    outPut("%s",source);
    delim=strchr(source,0);
    delim--;
    if(*delim=='\n'){
      *delim=0;
      delim--;
      if(*delim=='\r') *delim=0;
      }
    while(*delim==' '){
      *delim=0;
      delim--;
      }   
    if(!nbTranslatorParseStmt(context,source,source,file,reStackP,nsub,level,depthP,xiParent,0,reFlags)){
      fclose(file);
      outBar();
      outMsg(0,'E',"Translator \"%s\" failed to load.",filename);
      return(0);
      }
    }
  fclose(file);
  outBar();
  outMsg(0,'I',"Translator \"%s\" loaded successfully.",filename);
  return(1);
  }

nbCELL nbTranslatorCompile(nbCELL context,int reFlags,char *filename){
  NB_Translator *translator,**translatorP;
  struct NB_XI *xi;
  NB_String *filenameObject;
  int depth;
  struct REGEXP_STACK reStack;
  int nsub[NB_TRANSLATOR_STACKSIZE];

  reStack.count=0;
  nsub[0]=0;
  nsub[1]=0;
  // see if this file is already loaded
  filenameObject=grabObject(useString(filename));
  for(translatorP=&nb_Translators;*translatorP!=NULL && filenameObject<(*translatorP)->filename;translatorP=(NB_Translator **)&((*translatorP)->object.next));
  if(*translatorP!=NULL && filenameObject==(*translatorP)->filename){
    dropObject(filenameObject);
    return((nbCELL)*translatorP);
    }
  xi=nbAlloc(sizeof(struct NB_XI));
  memset(xi,0,sizeof(struct NB_XI));
  xi->item.cell=(nbCELL)filenameObject;
  xi->oper=NB_XI_OPER_FILE;
  if(!nbTranslatorInclude(context,filename,&reStack,nsub,1,&depth,xi,reFlags)){
    dropObject(filenameObject);
    nbTranslatorFreeList(xi);
    return(NULL);
    }
  translator=newObject(nb_TranslatorType,(void **)&nb_TranslatorFree,sizeof(NB_Translator));
  translator->xi=xi;
  translator->filename=grabObject(filenameObject);
  translator->depth=depth;
  translator->object.next=(NB_Object *)*translatorP;
  *translatorP=translator;
  return((nbCELL)translator);
  }

int nbTranslatorRefresh(nbCELL context,nbCELL translatorCell){
  NB_Translator *translator=(NB_Translator *)translatorCell;
  struct NB_XI *xiNode;
  int depth;
  struct REGEXP_STACK reStack;
  int nsub[NB_TRANSLATOR_STACKSIZE];

  reStack.count=0;
  nsub[0]=0;

  xiNode=nbAlloc(sizeof(struct NB_XI));
  memset(xiNode,0,sizeof(struct NB_XI));
  xiNode->oper=NB_XI_OPER_FILE;
  if(!nbTranslatorInclude(context,translator->filename->value,&reStack,nsub,1,&depth,xiNode,translator->reFlags)){
    nbTranslatorFreeList(xiNode);
    return(-1);
    }
  nbTranslatorFreeList(translator->xi->nest);
  translator->xi->nest=xiNode->nest;
  nbFree(xiNode,sizeof(struct NB_XI));
  translator->depth=depth;  // This is risky.  We need to fuss depth all the way up.
  return(0);
  }

int nbTranslatorDo(nbCELL context,nbCELL translatorCell,char *text){
  NB_Translator *translator=(NB_Translator *)translatorCell;
  char source[NB_BUFSIZE];
  struct REGEXP_STACK reStack;
  int nsub[NB_TRANSLATOR_STACKSIZE],level=1,depth=0,flag=0;
  FILE *file=NULL;

  reStack.count=0;
  nsub[0]=0;
  if(!nbTranslatorParseStmt(context,text,source,file,&reStack,nsub,level,&depth,translator->xi,flag,translator->reFlags)) return(-1);
  return(0);
  }
