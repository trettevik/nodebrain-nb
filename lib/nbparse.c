/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbparse.c 
*
* Title:    Header for Parsing Functions 
*
* Function:
*
*   This file provides routines for parsing nodebrain expressions and
*   translating them into internal form.
*
*==============================================================================
* Defects:
*
* o We have an ambiguity in the syntax
*
*         a=~"^abc.*"  [Regular expression match]
*         a~"^abc.*"   [Regular expression match]
*         a~b          [Assume b is a regular expression]
*
*         a=~(mo)      [assign the value of a schedule]
*         a~T(10s)     [Time delay
*         a~T("abc")   [T is a function that returns a regular expression?]
*
*  Proposed change
*
*         New Syntax   Replaced
*         -----------  ---------
*         a~~"^abc.*"  a=~"^abc.*"   Regular expression match
*                      a~"^abc.*"
*         a~0(10s)     a~F(10s)      False delay
*         a~1(10s)     a~T(10s)      True delay
*         a~?(10s)     a~U(10s)      Unknown delay
*
*         ~(...)                     Schedule
*         a=~(...)                   Assign value of schedule
*         a==~(...)                  Define a as schedule
*
*         ~=a          a~~           Change to a
*         ~=(...)      (...)~~       Change to cell expression
*
*  Planned for future
*
*         ~==a         Change to the definition of a
*
*         =a           Asserted value, even if no change
*         ==a          Asserted definition even if no change
*
*         ?a           True if a is Unknown
*         ??a          True if a has an Unknown definition
*
*         a?           Reserved for instantiation variables
*
*==============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/08/25 Ed Trettevik (original version split out of nblogic.c)
* 2002/09/05 eat - version 0.4.1 A1
*            1) Moved more parsing routines here from nodebrain.c
*
* 2002/09/22 eat - version 0.4.1 A4
*            1) New operators - !!, ??, !?, [], !&, !|
* 2002/10/11 eat - version 0.4.1 A7
*            1) New operator -  |!&  xor
* 2002/11/19 eat - version 0.4.2 B2
*            1) Modified isAlpha() to properly recognize '~' (tilde) on
*               EBCDIC machines.  This is not a complete solution, as the
*               ranges a-z and A-Z include non-alpha codes in EBCDIC.
* 2005/05/12 eat 0.6.3  nbParseAssertion() modified to accept ':' as delimiter
*            This resolves a defect---no change to documentation.
* 2005/05/14 eat 0.6.3  Removed '_' from alpha and alphanumeric character sets
*            We still want to recognize the underscore as a valid qualifier
*            prefix, just not as a valid character within an identifier.  This
*            is because we want to reserve it as a special qualifier for use
*            with skill modules.
*
*                <node>_<talent>(<args>)
*                <node>_<cell>
*
* 2008/11/06 eat 0.7.3  Converted to PCRE's native API
* 2009/10/28 eat 0.7.6  Recognize a single period "." as a term - fixing a bug
* 2009/10/28 eat 0.7.6  Recognize real numbers without lead digit - e.g. .35 is same as 0.35
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/25 eat 0.7.9  Found and fixed a bug in processing of xor and nor 
*                       > define r1 on(a xor b);
*                       > show r1
*                       Was:
*                         r1 = ! == on(a)
*                       Is:
*                         r1 = ! == on(a|!&b);
* 2010/02/25 eat 0.7.9  Found and fixed a bug in processing of [] closed world
*                       > define r1 on(![]a);
*                       > show r1
*                       Was:
*                         r1 = ! == on(0);
*                       Is:
*                         r1 = ! == on(!([]a)); 
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2010-06-12 eat 0.8.2  Included grapObject call in "value of" $() parsing
*                       This avoids a bad destroyReal() call on this:
*                       define a cell x+y;
*                       assert x,y;
*                       define z cell c and $(a);
* 2010-09-17 eat 0.8.3  fixed crash on unbalanced parens or braces in nbParseTimeSymbol
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
* 2013-04-06 eat 0.8.15 Renamed getQualifier to nbParseQualifier
* 2013-04-06 eat 0.8.15 Added size parameter to nbParseSymbol and nbParseQualifier
* 2013-04-06 eat 0.8.15 Added size parameter to nbParseTerm and nbParseTimeSymbol
* 2014-01-25 eat 0.9.00 Checker updates
* 2014-04-06 eat 0.9.01 "Assume False" operator -? replaces [] (closed word)
* 2014-04-06 eat 0.9.01 "Assume True" operator +? is new
* 2014-04-06 eat 0.9.01 Delay false operator ~^!() replaces ~^0()
* 2014-04-18 eat 0.9.01 Included conditional operators
* 2014-09-14 eat 0.9.03 Recognizing '_' as alternative to '.' as a term qualifier separator
*                Under this scheme, a '.' will imply a node and a '_' will imply a
*                non-node term.  We switched the facet indicator from '_' to '$' for
*                now because it is still undocumented.
* 2014-10-05 eat 0.9.03 Removed unused code for old =.= reference operator
*==============================================================================
*/
#include <nb/nbi.h>

int parseTrace=0;        /* debugging trace flag for parsing routines */

unsigned char nb_CharClass[256];  // see nbparse.h for definitions
//static unsigned char nb_CharClassPrefix[256];  // see nbparse.h for definitions

void nbParseInit(void){
  unsigned int i;
  char *numChar="0123456789";
  char *alphaChar="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  //char *leadingChar="_.@%$";
  char *leadingChar="_.%$";
  char *relationChar="=<>";
  char *comboChar="&?[|+-"; // single character operators that can start combo operators
  //char *soloChar="^(,{/*`";   // single character operators - consumed
  char *soloChar="^(,{/*`@";   // single character operators - consumed
  char *delimChar="])}:;";   // cell delimiter - not consumed 

  memset(nb_CharClass,(unsigned char)255,sizeof(nb_CharClass));
  for(i=0;i<strlen(numChar);i++) nb_CharClass[(int)*(numChar+i)]=NB_CHAR_NUMBER;
  for(i=0;i<strlen(alphaChar);i++) nb_CharClass[(int)*(alphaChar+i)]=NB_CHAR_ALPHA;
  nb_CharClass['\'']=NB_CHAR_TERMQUOTE;
  nb_CharClass['\"']=NB_CHAR_QUOTE;
  for(i=0;i<strlen(leadingChar);i++) nb_CharClass[(int)*(leadingChar+i)]=NB_CHAR_LEADING;
  for(i=0;i<strlen(relationChar);i++) nb_CharClass[(int)*(relationChar+i)]=NB_CHAR_RELATION;
  nb_CharClass['~']=NB_CHAR_TILDE;
  nb_CharClass['!']=NB_CHAR_NOT;
  for(i=0;i<strlen(comboChar);i++) nb_CharClass[(int)*(comboChar+i)]=NB_CHAR_COMBO;
  for(i=0;i<strlen(soloChar);i++) nb_CharClass[(int)*(soloChar+i)]=NB_CHAR_SOLO;
  for(i=0;i<strlen(delimChar);i++) nb_CharClass[(int)*(delimChar+i)]=NB_CHAR_DELIM;
  nb_CharClass['\n']=NB_CHAR_END;
  nb_CharClass[0]=NB_CHAR_END;
  //memcpy(nb_CharClassPrefix,nb_CharClass,256);
  //nb_CharClassPrefix[(int)'*']=NB_CHAR_LEADING;  // '*' is leading char as prefix
  }

/*
*  Parse a term (single level of qualification)
*
*  Returns length of qualifier copied
*
*  2005/05/14 eat 0.6.3  We no longer handle special first symbols of identifiers here
*             Characters like '%', '$', '@' are handled by nbParseSymbol()
*             Here we do recognize '_' as a valid first character for a qualifier and
*             we recognize "'" as a valid first character of a single quoted qualifier
*             We also accept leading periods "." here. This may not be the appropriate
*             place for that, but it works for now.
*/ 
static int nbParseTerm(char **termP,size_t size,char **cursorP){
  char *term=*termP,*cursor=*cursorP;
  size_t len;
  
  *term=0;
  while(*cursor=='.') cursor++;  // accept leading periods on a term qualifier
  //if(*cursor=='_' || *cursor=='*') cursor++;     // accept leading underscore or star
  if(*cursor=='_') cursor++;     // accept leading underscore
  if(*cursor=='\''){
    cursor++;
    while(*cursor!='\'' && *cursor!=0 && *cursor!='\n') cursor++;
    if(*cursor!='\''){
      outMsg(0,'E',"Qualifier with unbalanced quotes at: %s",*cursorP);
      return(0);
      }
    cursor++;
    }
  else if(NB_ISALPHA((int)*cursor)){
    cursor++;
    while(NB_ISALPHANUMERIC((int)*cursor)) cursor++;
    }
  len=cursor-*cursorP;
  if(len>=size){
    outMsg(0,'E',"Term length exceeds buffer size"); 
    return(0);
    }
  strncpy(term,*cursorP,len);
  term+=len;
  *term=0;
  *termP=term;
  *cursorP=cursor;
  return(len);  
  } 

/*
*  This is a relaxed term parser that allows a term to start with some
*  special characters and consumes a trailing period.  This is used to step
*  through an identifier one term at a time.  The symbol parser should ensure
*  that the special symbols will not prefix any subordinate term.
*
*  We need to review the use of these special prefixes within term glossaries.
*/
char *nbParseQualifier(char *qCursor,size_t size,char *sCursor){
  if(*sCursor=='@' || *sCursor=='$' || *sCursor=='%'){
    *qCursor=*sCursor;
    qCursor++;
    sCursor++;
    // Give special attention to @. or $. or %.
    // In these cases just return the single symbol
    if(*sCursor=='.' || *sCursor==0) *qCursor=0;
    else if(nbParseTerm(&qCursor,size-1,&sCursor)==0) return(NULL);  // 2012-12-27 eat - CID 751522 - intentional
    }
  else if(nbParseTerm(&qCursor,size,&sCursor)==0) return(NULL);
  //if(*sCursor=='.' || *sCursor=='_') sCursor++; /* step over one trailing period */  
  return(sCursor);  
  }

/*
*  Parse time condition symbol - just looking for balanced brackets
*/
static void nbParseTimeSymbol(char *symid,char *ident,size_t size,char **sourceP){
  int paren=1;
  char *cursor=*sourceP,*start,*end;
  size_t len;

  *ident=0;
  if(*cursor=='{'){   /* handle schedule plans */
    start=cursor;
    cursor++;
    while(paren>0 && *cursor!=0){
      if(*cursor=='{') paren++;
      else if(*cursor=='}') paren--;
      cursor++;
      }
    end=cursor;
    }
  else if(*cursor=='('){
    cursor++;
    start=cursor; // don't return the paren
    while(paren>0 && *cursor!=0){
      if(*cursor=='(') paren++;
      else if(*cursor==')') paren--;
      cursor++;
      }
    end=cursor-1;  // drop trailing parenthesis from identifier
    }
  else{
    outMsg(0,'E',"Time expression must start with '(' or '{' symbol at--> %s",*sourceP);
    *symid='.';
    return;
    }
  if(paren>0){
    outMsg(0,'E',"Unbalanced parentheses in time condition \"%s\".",*sourceP);
    *symid='.';
    return;
    }
  len=end-start;
  if(len>=size){
    outMsg(0,'E',"Time condition exceeds buffer size at--> %s",start);
    *symid='.';
    return;
    }
  strncpy(ident,start,len);
  *(ident+len)=0;
  *sourceP=cursor;
  }

/*
*  Symbol Parser
*
*  Returns char, symbol, and updates cursor.  The first
*  set of characters are delimiters.  They are not consumed
*  by the cursor, making them available to higher level
*  routines that only have access via the cursor.  A zero
*  length symbol is returned for these delimiters.
*
* S --- Symbol request    nbParseSymbol() 
* I --- Infix request     nbParseSymbolInfix()
* B --- Both
*
*    X --- symid returned
*
*        sssssssssssssss --- string recognized
*
*                        Name                      Modificatin Versions
*                        ------------------------  -------------------------
* B  .                   Syntax error              0.4.1 - changed from 255
*
* B  ;   ;,\n,\0         end of statement          0.4.1
* B  )   )               right paren
* B  }   }               right brace               0.2.8
* B  ]   ]               right bracket             0.2.9
* B  :   :               semi-colon delimiter
*
* S  $   $(              Value substitution within cell expression - "value of" 
* S  %   %(              Macro expansion where verb expected 
*
* S  u   ??              Unknown value             0.7.3 0.6.0 Supporting for compatibility
* S  i   integer number
* S  r   real number
* S  s   string literal
* S  t   $,@,%,.,_,alpha term identifier
*        alphanumeric    must start with special symbol or alpha, followed by alphanumerics
*                        may contain multiple terms separated by periods "."
*
* B  +   +               addition
* B  -   -               subtraction
* B  *   *               multiplication
* B  /   /               division
*
* S  =   =,==            assignment operator       0.7.3
* I  =   =,<=,>=,<>      relational operators
* I  m   ~          ~~   regex match               0.7.3  0.2.8
* I  c   ~?         ~=   change                    0.7.3  0.2.8
* S  ~   ~()             time condition
* I  9   ~^1()      ~T() delay true condition      0.7.3  Deprecated in 0.9.01
* I  T   ~^()       ~T() delay true condition      0.9.01  Replaces ~^1()
* I  0   ~^0()      ~F() delay false condition     0.7.3  Deprecated in 0.9.01
* I  F   ~^!()      ~F() delay false condition     0.9.01  Replaces ~^0()
* I  U   ~^?()      ~U() delay unknown condition   0.7.3
*
* S  !   !               Not operator 
* S  ?   ?               Unknown operator or value 0.6.0  use (?) to force it to be a value
* S  1   !!              True operator             0.4.1
* S  k   !?              Known operator            0.4.1
* S  w   []              closed world operator     0.4.1  Deprecated in 0.9.01
* S  w   -?              False if Unknown          0.9.01  Replaces closed world operator
* S  W   +?              True if Unknown           0.9.01
*
* I  A   &&              lazy and                  0.7.3
* B  &   &,and
* I  a   !&,nand         nand                      0.4.1
* I  O   ||              lazy or                   0.7.3
* B  |   |,or
* I  o   !|,nor          nor                       0.4.1
* I  x   |!&,xor         exclusive or              0.4.1
* I  E   &~&             and enable                0.7.3
* I  e   |~|             or enable                 0.7.3
* I  V   &^&             and value capture         0.7.3
* I  v   |^|             or value capture          0.7.3
* B  ^   ^               flip-flop
* B  (   (               left paren
* B  {   {               left brace                0.2.8
* B  [   [               left bracket              0.2.9
* B  ,   ,               comma
*
* I  2   true    !!      conditional operators     0.9.01
* I  3   untrue
* I  4   false    !
* I  5   unfalse
* I  6   known
* I  7   unknown  ?
* I  8   else
* I  G   elsetrue        
* I  H   elsefalse
* I  I   elseunknown
*
* 2005/05/14 eat 0.6.3  leading symbols for terms are handled differently now
* 2006/01/06 eat 0.6.4  recognize null delimiter (0) as term delimiter after "."
* 2008-11-11 eat 0.7.3  Split infix operators out to nbParseSymbolInfix()
*/
char nbParseSymbol(char *symbol,size_t size,char **source){
  char *cursor,*start,*symcur=symbol,symid='.';
  size_t len;

  if(parseTrace) outMsg(0,'T',"nbParseSymbol called [%s].",*source);
  if(size==0) outMsg(0,'E',"Symbol too long for buffer");
  cursor=*source;
  *symbol=0;
  while(*cursor==' ') cursor++;
  start=cursor;
  //switch(nb_CharClassPrefix[(int)*cursor]){
  switch(nb_CharClass[(int)*cursor]){
    case NB_CHAR_NUMBER: // handle numbers - integer and real 
      while(NB_ISNUMERIC((int)*cursor)) cursor++;
      symid='i';
      if(*cursor=='.'){
        cursor++;
        while(NB_ISNUMERIC((int)*cursor)) cursor++;
        symid='r';
        }
      if(*cursor=='e' && (*(cursor+1)=='+' || *(cursor+1)=='-') && NB_ISNUMERIC((int)*(cursor+2))){
        cursor+=3;
        while(NB_ISNUMERIC((int)*cursor)) cursor++;
        symid='r';
        }
      len=cursor-start;
      if(len<size){
        strncpy(symbol,start,len);
        *(symbol+len)=0;
        }
      else{
        outMsg(0,'E',"Symbol too long for buffer");
        symid='.';
        }
      break;
    case NB_CHAR_ALPHA:      // handle words and identifiers including single quoted strings
    case NB_CHAR_TERMQUOTE:
      if((len=nbParseTerm(&symcur,size,&cursor))==0) return('.');
      size-=len;  // size is always at least one here
      symid='t';
      if(*cursor=='.' || *cursor=='_'){
        while(*cursor=='.' || *cursor=='_'){
          *symcur=*cursor;
          symcur++;
          size--;  // we know size is at least one here
          cursor++;
          if((len=nbParseTerm(&symcur,size,&cursor))==0) symcur--;
          }
        if(*symcur=='.') cursor--; // 2006-01-06 eat 0.6.4 - backup when term ends with period
        }
      else if(strcmp(symbol,"not")==0) symid='!';
      break;
    case NB_CHAR_LEADING:     // leading symbol for special identifiers
      if(*cursor=='$' && *(cursor+1)=='('){
        symid='$';
        strncpy(symbol,cursor,2);
        *(symbol+2)=0;
        cursor+=2;
        }
      else if(*cursor=='%' && *(cursor+1)=='('){
        symid='%';
        strncpy(symbol,cursor,2);
        *(symbol+2)=0;
        cursor+=2;
        }
      else{
        *symcur=*cursor;
        symcur++;
        size--;    // size must be at least one
        cursor++;
        // 2009-10-28 eat - included support for real numbers starting with "." (e.g. .35)
        if(*symbol=='.' && NB_ISNUMERIC((int)*cursor)){
          symid='r';
          start=cursor;
          while(NB_ISNUMERIC((int)*cursor)) cursor++;
          if(*cursor=='e' && (*(cursor+1)=='+' || *(cursor+1)=='-') && NB_ISNUMERIC((int)*(cursor+2))){
            cursor+=3;
            while(NB_ISNUMERIC((int)*cursor)) cursor++;
            }
          len=cursor-start;
          if(len<size){
            strncpy(symcur,start,len);
            *(symcur+len)=0;
            }
          else{
            outMsg(0,'E',"Symbol too long for buffer");
            symid='.';
            }
          }
        else{
          symid='t';
          if((len=nbParseTerm(&symcur,size,&cursor))>0){
            size-=len;
            while((*cursor=='.' || *cursor=='_')  && (len=nbParseTerm(&symcur,size,&cursor))>0) size-=len;
            }
          // 2009-10-28 eat - included test on symbol to not treat single period as a trailing period
          if(symcur>symbol+1 && *(symcur-1)=='.'){  // step back from trailing period
            symcur--;
            cursor--;
            }
          *symcur=0;
          }
        }
      break;
    case NB_CHAR_QUOTE:    // handle string literals
      cursor++;
      start=cursor;
      while(*cursor!='\"' && *cursor!=0 && *cursor!='\n') cursor++;
      len=cursor-start;
      if(len>=size) outMsg(0,'E',"String too long for buffer");
      else if(*cursor=='\"'){
        strncpy(symbol,start,len);
        *(symbol+len)=0;
        cursor++;
        symid='s';
        }
      else{
        outMsg(0,'E',"End quote not found [%s].",*source);
        symid='.';
        }
      break;
    case NB_CHAR_RELATION:   // handle assignment operator
      if(*cursor=='='){
        symid='=';
        *symcur=*cursor; symcur++; cursor++;
        if(*cursor=='='){
          *symcur=*cursor; symcur++; cursor++; 
          }
        *symcur=0;
        }
      else symid='.';
      break;
    case NB_CHAR_TILDE:
      symid='~';
      cursor++;
      /* handle change */
      if(*cursor=='='){
        symid='c';
        *symcur=*cursor; symcur++; cursor++;
        *symcur=0;
        break;
        }
      while(*cursor==' ') cursor++;
      if(*cursor=='(' || *cursor=='{') nbParseTimeSymbol(&symid,symcur,size,&cursor);
      else symid='.';
      break;
    case NB_CHAR_NOT:     // handle operators starting with "!"
      symid='!'; 
      *symcur=*cursor; symcur++; cursor++;
      if(*cursor=='!') symid='1';                // !!
      else if(*cursor=='?') symid='k';           // !?
      if(symid!='!'){
        *symcur=*cursor; symcur++; cursor++;
        }
      *symcur=0;
      break;
    case NB_CHAR_COMBO:    // handle combination operators and related single char operators
      /* handle unknown value */
      if(*cursor=='?' && *(cursor+1)=='?'){
          symid='u';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          outMsg(0,'W',"Replace deprecated '?\?' with '?' or '(?)'."); // escape to avoid a trigraph warning
          *symcur=0;
          break;
          }
      // The []e closed world syntax is deprecated but still supported
      // The -?e syntax replaces Unknown the False -  same as e?!
      // The +?e syntax replaces Unknown the True  -  same as e?1
      if(((*cursor=='-' || *cursor=='+') && *(cursor+1)=='?') || (*cursor=='[' && *(cursor+1)==']')){
        if(*cursor=='[') outMsg(0,'W',"Replace deprecated [] closed world operator with -?");
        if(*cursor=='+') symid='W';
        else symid='w';
        *symcur=*cursor; symcur++; cursor++;
        *symcur=*cursor; symcur++; cursor++;
        *symcur=0;
        break;
        }
      // fall through to handle single character when combo not found
    case NB_CHAR_SOLO:    // handle characters not combining with others
      symid=*cursor;
      *symcur=*cursor; symcur++; cursor++;
      *symcur=0;
      break;
    case NB_CHAR_DELIM:
      symid=*cursor;
      break;
    case NB_CHAR_END:
      symid=';';
      break;
    default:
      symid='.'; 
    }
  *source=cursor;
  if(parseTrace) outMsg(0,'T',"nbParseSymbol returning ['%c',\"%s\"] [%s].",symid,symbol,cursor);
  return(symid);
  }

/*
*  Symbol Parser (Infix)
*
*    See comments above with nbParseSymbol
*/
static char nbParseSymbolInfix(char *symbol,size_t size,char **source){
  char *cursor,*start,*symcur=symbol,symid='.';
  size_t len;

  if(parseTrace) outMsg(0,'T',"nbParseSymbolInfix called [%s].",*source);
  cursor=*source;
  *symbol=0;
  while(*cursor==' ') cursor++;
  start=cursor;
  switch(nb_CharClass[(int)*cursor]){
    case NB_CHAR_ALPHA:      // handle words and identifiers including single quoted strings
      while(NB_ISALPHA((int)*cursor)) cursor++;
      len=cursor-start;
      if(len<size){
        strncpy(symbol,start,len);
        *(symbol+len)=0;
        if(strcmp(symbol,"and")==0) symid='&';
        else if(strcmp(symbol,"nand")==0) symid='a';
        else if(strcmp(symbol,"or")==0) symid='|';
        else if(strcmp(symbol,"nor")==0) symid='o';
        else if(strcmp(symbol,"xor")==0) symid='x';
        // 2014-04-18 eat - conditional operators
        else if(strcmp(symbol,"true")==0) symid='2';
        else if(strcmp(symbol,"untrue")==0) symid='3';
        else if(strcmp(symbol,"false")==0) symid='4';
        else if(strcmp(symbol,"unfalse")==0) symid='5';
        else if(strcmp(symbol,"known")==0) symid='6';
        else if(strcmp(symbol,"unknown")==0) symid='7';
        else if(strcmp(symbol,"else")==0) symid='J';
        // 2014-07-19 eat - extra else values
        else if(strcmp(symbol,"elsetrue")==0) symid='G';
        else if(strcmp(symbol,"elsefalse")==0) symid='H';
        else if(strcmp(symbol,"elseunknown")==0) symid='I';
        // 2014-04-25 eat - enabled monitoring and value capture
        else if(strcmp(symbol,"then")==0) symid='E';
        else if(strcmp(symbol,"capture")==0) symid='V';
        }
      break;
    case NB_CHAR_RELATION:   // handle relational operators
      while(*cursor!=0 && strchr("=<>",*cursor)!=NULL) cursor++;
      len=cursor-start;
      if(len<size){
        strncpy(symbol,start,len);
        *(symbol+len)=0;
        symid='=';
        }
      break;
    case NB_CHAR_TILDE:
      /* handle regular expression match */
      if(*(start+1)=='~'){
        strncpy(symbol,start,2);
        *(symbol+2)=0;
        symid='m';
        }
      /* handle time condition (prefix) and match (infix) conditions */
      else{
        symid='~';
        cursor++;
        if(*cursor=='^'){  // new syntax time delay: ~^(...) ~^!(...) ~^?(...)
          // 2014-04-27 eat - This will get cleaned up when the deprecated syntax is dropped
          cursor++;
          if(*cursor=='(') symid='T';
          else if(*cursor=='1') symid='9'; // 2014-04-27 eat - deprecated 'T'
          else if(*cursor=='0') symid='0'; // 2014-04-06 eat - deprecated 'F'
          else if(*cursor=='!') symid='F'; 
          else if(*cursor=='?') symid='U';
          else symid='.';
          if(*cursor!='(') cursor++;
          if(symid=='9') nbParseTimeSymbol("T",symbol,size,&cursor);
          else if(symid=='0') nbParseTimeSymbol("F",symbol,size,&cursor);
          else if(symid!='.') nbParseTimeSymbol(&symid,symbol,size,&cursor);
          }
        // support deprecated syntax
        //else if(*cursor=='T' || *cursor=='F' || *cursor=='U'){
        //  symid=*cursor;
        //  cursor++;
        //  nbParseTimeSymbol(&symid,symbol,size,&cursor);
        //  }
        else symid='m';
        }
      break;
    case NB_CHAR_NOT:     // handle operators starting with "!"
      symid='!';
      *symcur=*cursor; symcur++; cursor++;
      if(*cursor=='&') symid='a';
      else if(*cursor=='|') symid='o';
      else if(*cursor=='!') symid='2';    // alias for "true" conditional
      if(symid!='!'){
        *symcur=*cursor; symcur++; cursor++;
        }
      else symid='4';     // alias for "false" conditional
      *symcur=0;
      break;
    case NB_CHAR_COMBO:    // handle combination operators and related single char operators
      if(*cursor=='|'){
        if(*(cursor+1)=='|'){  // lazy and
          symid='O';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        else if(*(cursor+1)=='~' && *(cursor+2)=='|'){ // |~| or enable monitor
          symid='e';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        else if(*(cursor+1)=='^' && *(cursor+2)=='|'){ // |^| or value capture
          symid='v';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        /* handle exclusive or |!& */
        else if(strncmp(cursor,"|!&",3)==0){
          symid='x';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        }
      else if(*cursor=='&'){
        if(*(cursor+1)=='&'){   // && lazy and
          symid='A';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        else if(*(cursor+1)=='~' && *(cursor+2)=='&'){  // &^& and value capture
          symid='E';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        else if(*(cursor+1)=='^' && *(cursor+2)=='&'){  // &^& and value capture
          symid='V';
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=*cursor; symcur++; cursor++;
          *symcur=0;
          break;
          }
        }
      // fall through to handle single character when combo not found
    case NB_CHAR_SOLO:    // handle characters not combining with others
      if(*cursor=='?') symid='7';   // alias for "unknown" conditional
      else symid=*cursor;
      *symcur=*cursor; symcur++; cursor++;
      *symcur=0;
      break;
    case NB_CHAR_DELIM:
      //outMsg(0,'T',"NB_CHAR_DELIM");
      symid=*cursor;
      break;
    case NB_CHAR_END:
      //outMsg(0,'T',"NB_CHAR_END");
      symid=';';
      break;
    default:
      symid='.';
    }
  if(symid=='.') cursor=*source;
  else *source=cursor;
  if(parseTrace) outMsg(0,'T',"nbParseSymbolInfix returning ['%c',\"%s\"] [%s].",symid,symbol,cursor);
  return(symid);
  }


/*
* Parse low level object expressions (objects, terms, calls)
*
*   This is level 8 from nbParseCell's perspective
*/
NB_Object *nbParseObject(NB_Term *context,char **cursor){
  char ident[1024],token[1024],facetIdent[256];
  void *right;
  NB_Object *object,*objhold;
  NB_Term *term=NULL;
  char *delim,msg[1024],*savecursor=*cursor;
  //struct TYPE *type;
  //int not=0;
  char symid;
  char isSentence=0;

  if(parseTrace) outMsg(0,'T',"nbParseObject(): called -->%s",*cursor);
  symid=nbParseSymbol(ident,sizeof(ident),cursor);
  if(parseTrace) outMsg(0,'T',"nbParseObject(): nbParseSymbol returned ['%c',\"%s\"]-->%s",symid,ident,*cursor);

  if(NB_ISCELLDELIM((int)symid)) return(NULL);
  //if(strchr(")}:;",symid)!=NULL) return(NULL);
  if(symid==','){(*cursor)--; return(NULL);}
  switch(symid){
    case '!': // !
      if(strcmp(ident,"not")==0){
        outMsg(0,'W',"The \"not\" operator is deprecated to avoid reserved terms.  Please use ! instead.");
        }
      else if(**cursor==' ') return(NB_OBJECT_FALSE);  // 2014-04-26 eat - space means it is not a prefix operator
      if((object=nbParseRel(context,cursor))==NULL) return(NB_OBJECT_FALSE); // 2013-12-05 eat - uncommented
      if(object==nb_Unknown) return(nb_Unknown);
      if(object==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
      if(object->value==object) return(NB_OBJECT_FALSE);
      return((NB_Object *)useCondition(condTypeNot,object,nb_Unknown));
    case '1': // !! true
      //if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if(**cursor==' ') return(NB_OBJECT_TRUE);
      if((object=nbParseRel(context,cursor))==NULL) return(NB_OBJECT_TRUE);
      if(object==nb_Unknown || object==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE); // false constant
      if(object->value==object) return(NB_OBJECT_TRUE);  // true constant
      return((NB_Object *)useCondition(condTypeTrue,object,nb_Unknown));
    case 'k': // !? known
      //if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if((object=nbParseRel(context,cursor))==NULL) return(nb_Unknown);
      if(object==nb_Unknown) return(NB_OBJECT_FALSE);   // unknown constant
      if(object->value==object) return(NB_OBJECT_TRUE); // known constant
      if(object->type==condTypeUnknown || object->type==condTypeKnown) return(NB_OBJECT_TRUE);
      return((NB_Object *)useCondition(condTypeKnown,object,nb_Unknown));
    case '?': // ? unknown
      //if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if(**cursor==' ') return(nb_Unknown); // 2014-04-26 eat - space means it is not a prefix operator
      if((object=nbParseRel(context,cursor))==NULL) return(nb_Unknown);
      if(object==nb_Unknown) return(NB_OBJECT_TRUE);     // we know unknown is unknown
      if(object->value==object) return(NB_OBJECT_FALSE); // other constants are known
      if(object->type==condTypeUnknown || object->type==condTypeKnown) return(NB_OBJECT_FALSE);
      return((NB_Object *)useCondition(condTypeUnknown,object,nb_Unknown));
    case 'w': // -? closed world false - [] closed world
      if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if(object==nb_Unknown) return(NB_OBJECT_FALSE);      // unknown is false in closed world
      if(object==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE); // false is false
      if(object->value==object) return(NB_OBJECT_TRUE);     // other constants are true
      return((NB_Object *)useCondition(condTypeAssumeFalse,object,nb_Unknown));
    case 'W': // +? assume true
      if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if(object==nb_Unknown) return(NB_OBJECT_TRUE);      // unknown is true
      if(object==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE); // false is false
      if(object->value==object) return(NB_OBJECT_TRUE);     // other constants are true
      return((NB_Object *)useCondition(condTypeAssumeTrue,object,nb_Unknown));
    case 'c':
      object=nbParseObject(context,cursor);
      if(object==NULL) return(NULL);
      return((NB_Object *)useCondition(condTypeChange,object,nb_Unknown));
    case '(': // parenthetical expression
      if((object=nbParseCell(context,cursor,0))==NULL) return(NULL);
      if(**cursor!=')'){
        outMsg(0,'E',"Unbalanced parentheses [%s].",*cursor);
        return(NULL);
        }
      *cursor=*cursor+1; /* consume right parenthesis */
      return(object);
    case '$': // value expression
      if((object=nbParseCell(context,cursor,0))==NULL) return(NULL);
      if(**cursor!=')'){
        outMsg(0,'E',"Unbalanced braces at-->%s",*cursor);
        return(NULL);
        }
      *cursor=*cursor+1; // consume right brace 
      /*
      *  Get the value of the object and then let the object go.
      *  The use of enable and disable is a bit inefficient because
      *  it unnecessarily registers objects for alerts and then
      *  unregisters them.  It would be better to set a mode flag so
      *  nbAxonEnable only calls the eval method when called by
      *  the eval methods, and doesn't do change registration.
      *  We can make that change if this code produces the desired
      *  results.
      */
      if(object->value==nb_Disabled){
        object->type->enable(object);
        objhold=object->type->eval(object);
        object->type->disable(object);
        }
      else objhold=object->value;
      if(objhold->type==realType){
        objhold=(NB_Object *)useReal(((struct REAL *)objhold)->value);
        }
      if(object->refcnt==0) object->type->destroy(object);
      return(objhold); 
    case '{':
      if(NULL==(object=(NB_Object *)nbRuleParse((nbCELL)context,0,cursor,msg,sizeof(msg)))) outPut("%s\n",msg);
      return(object);
    case '~': // time condition 
      if(parseTrace) outMsg(0,'T',"Calling newSched A [%s].",ident);
      if(NULL==(object=(NB_Object *)newSched((nbCELL)context,symid,ident,&delim,msg,sizeof(msg),1))){
        outPut("%s\n",msg);
        *cursor=savecursor;
        return(NULL);
        }
      if(parseTrace) outMsg(0,'T',"Schedule structure generated.");
      return((NB_Object *)useCondition(condTypeTime,nb_Unknown,object));
    case '-':
    case '+': // number 
      savecursor=*cursor;
      *token=symid;
      symid=nbParseSymbol(ident,sizeof(ident),cursor); 
      if(symid=='r' || symid=='i'){
        strcpy(token+1,ident);
        return((NB_Object *)parseReal(token));
        }
      *cursor=savecursor;
      if(*token=='+') return(nbParseObject(context,cursor));
      object=nbParseObject(context,cursor);
      if(object!=NULL) return((NB_Object *)useCondition(mathTypeInv,nb_Unknown,object));
      *cursor=savecursor;
      return(NULL);
    case 'u': // unknown
      return(nb_Unknown);
    case 'r':
    case 'i': // number
      return((NB_Object *)parseReal(ident));
    case 's': // string literal 
      return((NB_Object *)useString(ident));
    case '`': // function call identifier
      savecursor=*cursor;
      symid=nbParseSymbol(ident,sizeof(ident),cursor);
      if(symid!='t'){
        outMsg(0,'E',"Expecting function name at-->%s",savecursor);
        return(NULL);
        }
      if(!NB_ISALPHA((int)*ident)){
        outMsg(0,'E',"Function name must start with alpha character at-->%s",savecursor);
        return(NULL);
        }
      if(**cursor!='('){
        outMsg(0,'E',"Expecting '(' at-->%s",*cursor);
        return(NULL);
        }
      (*cursor)++;
      right=parseList(context,cursor);
      symid=nbParseSymbol(token,sizeof(token),cursor);
      if(symid!=')'){
        outMsg(0,'E',"Expecting \")\" at end of parameter list.");
        return(NULL);
        }
      (*cursor)++;
      object=NbCallUse((nbCELL)context,ident,(NB_List *)right); // create a new call object
      if(object) return(object);
      outMsg(0,'E',"Cell function %s not defined",ident);
      return(NULL);
    case '@': // facet identifier
      term=addrContext;    // term implied if starting with facet references
      if(NB_ISALPHA((int)**cursor)){
      //if(**cursor!='('){
        savecursor=*cursor;
        symid=nbParseSymbol(facetIdent,sizeof(facetIdent),cursor);
        if(symid!='t'){ 
          outMsg(0,'E',"Expecting facet at-->%s",savecursor);
          return(NULL);
          }
        }
      isSentence=1;
    case 't': // term identifier 
      if(!term){ // did not fall through from '@' facet above
        // A single underscore identifies the placeholder cell
        // Perhaps this determination should be moved to nbParseSymbol
        if(*ident=='_' && *(ident+1)==0) return(nb_Placeholder);
  
        if(parseTrace) outMsg(0,'T',"nbParseObject: parsed term \"%s\"",ident);
        term=nbTermFind(context,ident);
        if(*ident=='$' || *ident=='%'){
          // 2006-12-22 eat - when ready, experiment with using nb_Disabled as definition for "undefined" terms
          //if(term==NULL || term->def==nb_Disabled){
          if(term==NULL || term->def==nb_Unknown){
            outMsg(0,'E',"Reference to undefined symbolic \"%s\"",ident);
            return(NULL);
            }
          return(term->def);
          }
        *facetIdent=0;     // assume no facet specified
        if(**cursor=='@'){ // facet assertion
          (*cursor)++;
          isSentence=1;
          if(NB_ISALPHA((int)**cursor)){
            savecursor=*cursor;
            symid=nbParseSymbol(facetIdent,sizeof(facetIdent),cursor);
            if(symid!='t'){
              outMsg(0,'E',"Expecting facet at-->%s",savecursor);
              return(NULL);
              }
            }
          }
        }
      /* default to cell term if not define */
      savecursor=*cursor;
      if(**cursor!='('){
        if(isSentence){
          if(!term){
            term=nbTermNew(context,ident,nbNodeNew(),1);
            ((NB_Node *)term->def)->context=term;
            }
          NB_Facet *facet=nbSkillGetFacet(((NB_Node *)term->def)->skill,facetIdent);
          if(!facet) facet=nbSkillGetFacet(nb_SkillDefault,facetIdent);
          if(!facet) facet=(NB_Facet *)nbSkillFacet((nbCELL)context,(nbCELL)nb_SkillUnknown,facetIdent);
          if(!facet){
            outMsg(0,'L',"Unable to create unknown facet \"%s\" for node \"%s\".",facetIdent,ident);
            return(NULL);
            }
          return((NB_Object *)nbSentenceNew(facet,term,NULL));
          }
        // 2006-12-22 eat - when ready, experiment with using nb_Disabled definition for "undefined" terms
        //if(term==NULL) term=nbTermNew(context,ident,nb_Disabled,1);
        if(term==NULL) term=nbTermNew(context,ident,nb_Unknown,1);
        return((NB_Object *)term);
        }
      (*cursor)++;
      right=parseList(context,cursor);
      symid=nbParseSymbol(token,sizeof(token),cursor);
      if(symid!=')'){
        outMsg(0,'E',"Expecting \")\" at end of parameter list.");
        return(NULL);
        }
      (*cursor)++;
      if(term==NULL){
        // 2014-09-13 eat - undefined term used as node, make it a node with default skill
        if(!isSentence){  // support deprecated syntax first
          // 2014-11-22 eat - This should be removed after a couple releases
          object=NbCallUse((nbCELL)context,ident,(NB_List *)right); // create a new call object
          if(object && !strchr(ident,'.') && strcmp(ident,"trace")!=0){
            outMsg(0,'W',"Deprecated syntax for built-in function.  Use `math.%s(...) instead.",ident);
            return(object);
            }
          }
        term=nbTermNew(context,ident,nbNodeNew(),1);
        ((NB_Node *)term->def)->context=term;
        }
      if(term->def->type==nb_NodeType){
        NB_Facet *facet=nbSkillGetFacet(((NB_Node *)term->def)->skill,facetIdent);
        if(!facet) facet=nbSkillGetFacet(nb_SkillDefault,facetIdent);
        if(!facet) facet=(NB_Facet *)nbSkillFacet((nbCELL)context,(nbCELL)nb_SkillUnknown,facetIdent);
        if(!facet){
          outMsg(0,'L',"Unable to create unknown facet \"%s\" for node \"%s\".",facetIdent,ident);
          return(NULL);
          }
        return((NB_Object *)nbSentenceNew(facet,term,right));
        }
      else{
        outMsg(0,'E',"Sentence requires node - \"%s\" not defined as node.",ident);
        return(NULL);
        }
    }
  (*cursor)=savecursor;
  return(NULL);
  }

/*
* Parse relational expressions
*
*   We are doing a few weird things to support old code using deprecated syntax.
*
*   This routine implements level 4 operators.
*/
NB_Object *nbParseRel(NB_Term *context,char **cursor){
  char symid,operator[256];
  NB_Object *lobject,*robject,*object;
  char *savecursor;
  struct TYPE *type;
  int not=0;

  if(parseTrace) outMsg(0,'T',"nbParseRel(): called [%s].",*cursor);
  if((lobject=nbParseCell(context,cursor,6))==NULL) return(NULL);
  savecursor=*cursor;
  symid=nbParseSymbolInfix(operator,sizeof(operator),cursor);
  if(parseTrace) outMsg(0,'T',"nbParseRel(): nbParseSymbol returned ['%c',\"%s\"].",symid,operator);
  if(strchr(")}]:;",symid)!=NULL) return(lobject);

  if(symid=='m'){
    type=condTypeMatch;
    if(strcmp(operator,"~~")==0) outMsg(0,'W',"Replace deprecated '~~' with '~'.");
    }
  else if(symid=='~'){symid='m'; type=condTypeMatch;}
  else if(symid=='='){
    if(strcmp(operator,"=")==0) type=condTypeRelEQ;
    else if(strcmp(operator,"<>")==0) type=condTypeRelNE;
    else if(strcmp(operator,"<")==0) type=condTypeRelLT;
    else if(strcmp(operator,"<=")==0) type=condTypeRelLE;
    else if(strcmp(operator,">")==0) type=condTypeRelGT;
    else if(strcmp(operator,">=")==0) type=condTypeRelGE;
    else {
      outMsg(0,'L',"Operator \"%s\" not recognized.",operator);
      return(NULL);
      }
    }
  else{
    (*cursor)=savecursor; /* pass it back if we don't get it */
    return(lobject);
    }
  if(symid=='m'){
    char token[1024];
    struct REGEXP *re;
    if((symid=nbParseSymbol(token,sizeof(token),cursor))!='s'){
      outMsg(0,'E',"Expecting string literal regular expression.");
      return(NULL);
      }
    re=newRegexp(token,PCRE_NO_AUTO_CAPTURE);
    /* let newRegexp print error messages for now.  In the future, we
    *  should return a message and offset. */
    if(re==NULL) return(NULL);
    if(parseTrace) outMsg(0,'T',"Encountered regular expression.");
    // 2014-01-25 eat - CID 1164443 code was changed above so not is always 0
    //if(not){
    //  lobject=(NB_Object *)useCondition(type,lobject,re);
    //  return((NB_Object *)useCondition(condTypeNot,lobject,nb_Unknown));
    //  }
    return((NB_Object *)useCondition(type,lobject,re));
    }
  if((robject=nbParseCell(context,cursor,6))==NULL){
    outMsg(0,'E',"Expecting right operand to relational operator at-->%s",cursor);
    /* dropObject(lobject); add this later */
    return(NULL);
    }
  if(lobject->value==lobject && robject->value!=robject){
    object=lobject;  // make sure the constant is on the right to simplify use of axon in enable method
    lobject=robject;
    robject=object;
    if(type==condTypeRelEQ || type==condTypeRelNE);
    else if(type==condTypeRelGT) type=condTypeRelLT;
    else if(type==condTypeRelGE) type=condTypeRelLE;
    else if(type==condTypeRelLT) type=condTypeRelGT;
    else if(type==condTypeRelLE) type=condTypeRelGE;
    }
  if(nb_opt_boolnotrel){  // option to transform relational not to boolean not
    if(type==condTypeRelNE){
      not^=1;  // exclusive or to switch not
      type=condTypeRelEQ;
      }
    if(type==condTypeRelLE){
      not^=1;  // exclusive or to switch not
      type=condTypeRelGT;
      }
    else if(type==condTypeRelGE){
      not^=1;  // exclusive or to switch not
      type=condTypeRelLT;
      }
    }
  if(not){
    lobject=(NB_Object *)useCondition(type,lobject,robject);
    return((NB_Object *)useCondition(condTypeNot,lobject,nb_Unknown));
    }
  return((NB_Object *)useCondition(type,lobject,robject));
  }


/*
* Parse cell expression
*
* Return: Object structure or NULL if syntax error.
*
*/
NB_Object *nbParseCell(NB_Term *context,char **cursor,int level){
  char operator[256];
  NB_Object *lobject,*robject,*sobject;
  char *delim,msg[1024];
  struct TYPE *type;
  char symid,*cursave;
  int conditionalState=0;  // 8 - decided, 4 - true, 2 - false, 1 - unknown
  NB_Object *cobject=NULL; // conditional condition object

  if(parseTrace) outMsg(0,'T',"nbParseCell(%d): called [%s].",level,*cursor);
  if(level==7) lobject=nbParseObject(context,cursor);
  else if(level==4) lobject=nbParseRel(context,cursor);
  else lobject=nbParseCell(context,cursor,level+1);
  if(lobject==NULL) return(NULL);
  while(1){
    if(parseTrace) outMsg(0,'T',"nbParseCell(%d): calling nbParseSymbolInfix [%s].",level,*cursor);
    cursave=*cursor;
    symid=nbParseSymbolInfix(operator,sizeof(operator),cursor);
    if(parseTrace) outMsg(0,'T',"nbParseCell(%d): back from nbParseSymbolInfix [%s].",level,*cursor);
    if(strchr(")}]:;",symid)!=NULL) return(lobject);
    if(symid=='.'){
      if(level!=0) return(lobject);
      outMsg(0,'E',"Operator not recognized at-->%s",cursave);
      return(NULL);
      }
    /* should , be a terminator like the above symbols and not be consumed? */
    if(symid==','){(*cursor)--; return(lobject);}
    robject=NULL;
    switch(level){
      case 0: // conditional
        if(symid<'2' || (symid>'7' && symid<'G') || symid>'j'){  // not a conditional operator
          *cursor=cursave;
          return(lobject);
          //outMsg(0,'E',"Operator \"%s\" not expected at-->%s",operator,cursave);
          //return(NULL);
          }
        if(symid>='G' && symid<='J'){ // elsetrue, elsefalse, elseunknown, else
          NB_Conditional *conditional=(NB_Conditional *)lobject;
          //if(!(conditionalState&8) && lobject->type!=nb_ConditionalType){  // go ahead and create a conditional - we can add code to make sure we really need to
          if(lobject->type!=nb_ConditionalType){  // go ahead and create a conditional - we can add code to make sure we really need to
            //conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)lobject,(nbCELL)lobject,(nbCELL)lobject);
            //lobject=(NB_Object *)conditional;
            //conditionalState|=4;
            outMsg(0,'E',"Operator \"%s\" not expected at-->%s",operator,cursave);
            return(NULL);
            }
          cursave=*cursor;
          robject=nbParseCell(context,cursor,1);
          if(robject==NULL){
            outMsg(0,'E',"Operator \"%s\" right side not valid at-->%s",operator,cursave);
            return(NULL);
            }
          if(!(conditionalState&8)) switch(symid){  // process if we have already decided branch
            case 'G':  // elsetrue
              if(conditionalState&4){
                outMsg(0,'E',"Operator \"%s\" repeated at-->%s",operator,cursave);
                return(NULL);
                }
              dropObject(conditional->ifTrue);
              conditional->ifTrue=(nbCELL)grabObject(robject);
              if(conditional->cell.object.value!=nb_Disabled) nbCellEnable(conditional->ifTrue,(nbCELL)conditional);
              break;
            case 'H':  // elsefalse
              if(conditionalState&2){
                outMsg(0,'E',"Operator \"%s\" repeated at-->%s",operator,cursave);
                return(NULL);
                }
              dropObject(conditional->ifFalse);
              conditional->ifFalse=(nbCELL)grabObject(robject);
              if(conditional->cell.object.value!=nb_Disabled) nbCellEnable(conditional->ifFalse,(nbCELL)conditional);
              break;
            case 'I':  // elseunknowe 
              if(conditionalState&1){
                outMsg(0,'E',"Operator \"%s\" repeated at-->%s",operator,cursave);
                return(NULL);
                }
              dropObject(conditional->ifUnknown);
              conditional->ifUnknown=(nbCELL)grabObject(robject);
              if(conditional->cell.object.value!=nb_Disabled) nbCellEnable(conditional->ifUnknown,(nbCELL)conditional);
              break;
            case 'J': // else
              if(!(conditionalState&4)){
                dropObject(conditional->ifTrue);
                conditional->ifTrue=(nbCELL)grabObject(robject);
                }
              if(!(conditionalState&2)){
                dropObject(conditional->ifFalse);
                conditional->ifFalse=(nbCELL)grabObject(robject);
                }
              if(!(conditionalState&1)){
                dropObject(conditional->ifUnknown);
                conditional->ifUnknown=(nbCELL)grabObject(robject);
                }
              break;
            }
          }
        else{  // condtional other than else
          cursave=*cursor;
          robject=nbParseCell(context,cursor,1);
          if(robject==NULL){
            outMsg(0,'E',"Operator \"%s\" right side not valid at-->%s",operator,cursave);
            return(NULL);
            }
          NB_Conditional *conditional=NULL;
          cobject=lobject;
          switch(symid){
            case '2':  // true
              conditionalState|=4;
              if(lobject->type->kind&NB_OBJECT_KIND_TRUE) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT || lobject==robject); // lobject is good enough
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)robject,(nbCELL)lobject,(nbCELL)lobject);
              break;
            case '3':  // untrue
              conditionalState|=3;
              if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT && !(lobject->type->kind&NB_OBJECT_KIND_TRUE)) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_TRUE || lobject==robject);
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)lobject,(nbCELL)robject,(nbCELL)robject);
              break;
            case '4':  // false
              conditionalState|=2;
              if(lobject->type->kind&NB_OBJECT_KIND_FALSE) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT || lobject==robject); // lobject is good enough
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)lobject,(nbCELL)robject,(nbCELL)lobject);
              break;
            case '5':  // unfalse
              conditionalState|=5;
              if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT && !(lobject->type->kind&NB_OBJECT_KIND_FALSE)) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_FALSE || lobject==robject);
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)robject,(nbCELL)lobject,(nbCELL)robject);
              break;
            case '6':  // known
              conditionalState|=6;
              if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT && !(lobject->type->kind&NB_OBJECT_KIND_UNKNOWN)) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_UNKNOWN || lobject==robject);
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)robject,(nbCELL)robject,(nbCELL)lobject);
              break;
            case '7':  // unknown
              conditionalState|=1;
              if(lobject->type->kind&NB_OBJECT_KIND_UNKNOWN) lobject=robject,conditionalState|=8;
              else if(lobject->type->kind&NB_OBJECT_KIND_CONSTANT || lobject==robject); // lobject is good enough
              else conditional=nbConditionalUse((nbCELL)lobject,(nbCELL)lobject,(nbCELL)lobject,(nbCELL)robject);
              break;
            }
          if(conditional) lobject=(NB_Object *)conditional;
          }
        break;
      case 1:
        if(symid=='|') type=condTypeOr;
        else if(symid=='O') type=condTypeLazyOr;
        else if(symid=='o') type=condTypeNor;
        else if(symid=='x') type=condTypeXor;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,1))!=NULL){
          if((type==condTypeOr || type==condTypeLazyOr) && (sobject=reduceOr(lobject,robject))!=NULL) lobject=sobject;
          else lobject=(NB_Object *)useCondition(type,lobject,robject);
          }
        break;
      case 2:
        if(symid=='&') type=condTypeAnd;
        else if(symid=='A') type=condTypeLazyAnd;
        else if(symid=='a') type=condTypeNand;
        else if(symid=='?') type=condTypeDefault;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,2))!=NULL){
          if(type==condTypeDefault){
            if(lobject==nb_Unknown) lobject=robject;        // use right if left is unknown
            else if(lobject->value==lobject || robject==nb_Unknown); // use left if constant left or unknown right
            else lobject=(NB_Object *)useCondition(type,lobject,robject);
            }
          else if((type==condTypeAnd || type==condTypeLazyAnd) && 
            (sobject=reduceAnd(lobject,robject))!=NULL) lobject=sobject;
          else lobject=(NB_Object *)useCondition(type,lobject,robject);
          }
        break;
      case 3:
        if(symid=='^') type=condTypeFlipFlop;
        else if(symid=='V') type=condTypeAndCapture;
        else if(symid=='v') type=condTypeOrCapture;
        else if(symid=='E') type=condTypeAndMonitor;
        else if(symid=='e') type=condTypeOrMonitor;
        else{*cursor=cursave; return(lobject);}
        if(symid=='v' || strcmp(operator,"&^&")==0){
          outMsg(0,'W',"Operator %s is deprecated, please use \"capture\" instead.",operator); 
          }
        else if(symid=='e' || strcmp(operator,"&~&")==0){
          outMsg(0,'W',"Operator %s is deprecated, please use \"then\" instead.",operator); 
          }
        if((robject=nbParseCell(context,cursor,4))!=NULL) lobject=(NB_Object *)useCondition(type,lobject,robject);
        break;
      case 4:
        if(symid=='T') type=condTypeDelayTrue;
        else if(symid=='F') type=condTypeDelayFalse;
        else if(symid=='U') type=condTypeDelayUnknown;
        else if(symid=='0'){
          symid='F';
          type=condTypeDelayFalse;
          outMsg(0,'W',"Operator ~^0(...) is deprecated, please use ~^!(...) instead.");
          }
        else if(symid=='9'){
          symid='T';
          type=condTypeDelayTrue;
          outMsg(0,'W',"Operator ~^1(...) is deprecated, please use ~^(...) instead.");
          }
        else{*cursor=cursave; return(lobject);}
        robject=(NB_Object *)newSched((nbCELL)context,symid,operator,&delim,msg,sizeof(msg),1);
        if(robject==NULL){
          outPut("%s\n",msg);
          return(NULL);
          }
        lobject=(NB_Object *)useCondition(type,lobject,robject); 
        break;
      /* level 5 is handled by nbParseRel */
      case 6:
        if(symid=='+') type=mathTypeAdd;
        else if(symid=='-') type=mathTypeSub;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,7))!=NULL) lobject=(NB_Object *)useMath(type,lobject,robject);
        break;
      case 7:
        if(symid=='*') type=mathTypeMul;
        else if(symid=='/') type=mathTypeDiv;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseObject(context,cursor))!=NULL) lobject=(NB_Object *)useMath(type,lobject,robject);
        break;
      /* level 8 is handled by nbParseObject */
      default:
        outMsg(0,'L',"nbParseCell(%d): Level not recognized",level);
        return(NULL);
      }
    if(robject==NULL){
      outMsg(0,'E',"Expecting right operand for '%c' operator.",symid);
      return(NULL);
      }
    }
  }

/*
*  Parse an assertion list: e.g. "a=1,b==a+c,c,!d,?e"
*
*    The termContext is used for the terms getting assigned new values or definitions.
*    The cellContext is used for the cell expressions on the right (when required).
*    These are only different in the construct {node|macro}(a,b,c,`x=a+b,c=7).
*
*    2005/04/09 eat 0.6.2  included support for implicit node assertions
*                          fred. assert ("abc"),("def");
*                          assert fred("abc"),fred("def");
*/
NB_Link *nbParseAssertion(NB_Term *termContext,NB_Term *cellContext,char **curP){
  char *cursor=*curP,*cursave,ident[256],ident2[256],facetIdent[256],symid=',';
  NB_Link *member=NULL,*entry,**next;
  NB_Term   *term;
  NB_List   *list=NULL;
  NB_Object *object,*objectL;
  NB_Facet  *facet;
  struct TYPE *type;
  char not,unknown;
  char isSentence=0;

  if(parseTrace) outMsg(0,'T',"nbParseAssertion() called");
  next=&member;
  while(symid==','){
    *facetIdent=0;  // clear facet identifier
    isSentence=0;
    *curP=cursor;
    symid=nbParseSymbol(ident,sizeof(ident),&cursor);
    if(symid=='!'){
      not=1;
      symid=nbParseSymbol(ident,sizeof(ident),&cursor);
      }
    else not=0;
    if(symid=='?'){
      unknown=1;
      symid=nbParseSymbol(ident,sizeof(ident),&cursor);
      }
    else unknown=0;
    // 2014-10-05 eat - switched from "_" to "@" for facets
    if(symid=='(' || symid=='@'){  /* allow for null term */
      *ident=0;
      cursor--;
      }
    else if(symid!='t'){
      outMsg(0,'E',"Expecting term at-->%s",*curP);
      return(NULL);
      }
    *curP=cursor;
    // 2014-10-05 eat - switched from "_" to "@" for facets
    if(*cursor=='@'){ // facet assertion
      cursor++;
      if(NB_ISALPHA((int)*cursor)){
        cursave=cursor;
        symid=nbParseSymbol(facetIdent,sizeof(facetIdent),&cursor);
        if(symid!='t'){
          outMsg(0,'E',"Expecting facet at-->%s",cursave);
          return(NULL);
          }
        }
      isSentence=1;
      }
    if(*cursor=='('){ /* node assertion */
      isSentence=1;
      cursor++;
      list=parseList(cellContext,&cursor);
      symid=nbParseSymbol(ident2,sizeof(ident2),&cursor);
      if(symid!=')'){
        outMsg(0,'E',"Expecting ')' at end of parameter list.");
        return(NULL);
        }
      cursor++;
      }
    while(*cursor==' ') cursor++;   /* allow white space */
    *curP=cursor;
    if(*cursor=='='){
      if(not || unknown){
        outMsg(0,'E',"Unexpected = with ! or ? operator at-->%s",cursor);
        return(NULL);
        }
      cursor++;
      // $x=a+b is an alternative to x==a+b
      // this is an experiment---probably not a good idea - don't document
      if(*ident=='$') type=assertTypeDef;
      else type=assertTypeVal;
      if(*cursor=='='){
        type=assertTypeDef;
        cursor++;
        }
      *curP=cursor;
      object=nbParseCell(cellContext,&cursor,0);
      }
    else if(*cursor==',' || *cursor==':' || *cursor==';' || *cursor==0){
      type=assertTypeVal;
      if(not) object=NB_OBJECT_FALSE;
      else if(unknown) object=nb_Unknown;
      else object=NB_OBJECT_TRUE;
      }
    else{
      outMsg(0,'E',"Expecting '=' ',' or ';' at-->\"%s\".",*curP);
      return(NULL);
      }
    if(object==NULL) return(NULL);
    if(*ident==0) term=termContext;
    else if((term=nbTermFind(termContext,ident))==NULL){
      if(*ident=='$'){
        if(*(ident+1)==0){
          outMsg(0,'E',"Expecting term after $");
          return(NULL);
          }
        term=nbTermNew(termContext,ident+1,nb_Unknown,1);
        }
      else if(*ident=='%'){
        if(*(ident+1)==0){
          outMsg(0,'E',"Expecting term after %");
          return(NULL);
          } 
        term=nbTermNew(symContext,ident+1,nb_Unknown,1);
        }
      else term=nbTermNew(termContext,ident,nb_Unknown,1);
      }
    //else if((term->def->type->attributes&TYPE_WELDED) && *facetIdent==0 && list==NULL){
    else if((term->def->type->attributes&TYPE_WELDED) && !isSentence){
      outMsg(0,'E',"Term \"%s\" is not open to assertion.",ident);
      return(NULL);
      }
    //if(*facetIdent || list!=NULL){ // sentence 
    if(isSentence){ // sentence 
      if(term->def==nb_Unknown){
        term->def=grabObject(nbNodeNew());
        ((NB_Node *)term->def)->context=term;
        }
      else if(term->def->type!=nb_NodeType){
        outMsg(0,'E',"Sentence requires node -  \"%s\" not defined as node.",ident);
        return(NULL);
        }
      facet=nbSkillGetFacet(((NB_Node *)term->def)->skill,facetIdent);
      if(!facet) facet=nbSkillGetFacet(nb_SkillDefault,facetIdent);
      if(!facet) facet=(NB_Facet *)nbSkillFacet((nbCELL)term,(nbCELL)nb_SkillDefault,facetIdent);
      if(!facet){
        outMsg(0,'E',"Unable to create unknown facet \"%s\" for node \"%s\".",facetIdent,ident);
        return(NULL);
        }
      objectL=(NB_Object *)nbSentenceNew(facet,term,list);
      object=(NB_Object *)useCondition(type,objectL,object);
      list=NULL;
      }
    else{
      object=(NB_Object *)useCondition(type,term,object);
      // If the term is directly within the context, then make it transient
      for(term=term->context;term && term->def->type!=nb_NodeType;term=term->context);
      if(term==termContext) ((NB_Cell *)object)->mode|=NB_CELL_MODE_TRANSIENT;
      }
    // 2012-10-13 eat - replaced malloc
    if((entry=nb_LinkFree)==NULL) entry=nbAlloc(sizeof(NB_Link));
    else nb_LinkFree=entry->next;
    *next=entry;
    entry->next=NULL;
    next=&(entry->next);
    entry->object=grabObject(object);
    /* may want to enable the object here
    * otherwise we can let nbTermAssign do it
    */
    *curP=cursor;
    symid=nbParseSymbol(ident,sizeof(ident),&cursor);
    }
  return(member);
  }

// Add an assertion to an assertion list 
//   This will be moved to nbassertion.c
int nbAssertionListAddTermValue(nbCELL context,nbSET *set,nbCELL term,nbCELL cell){
  NB_Link   *entry;
  NB_Object *object;
  object=(NB_Object *)useCondition(assertTypeVal,term,cell);
  // 2012-10-13 eat - replace malloc
  if((entry=nb_LinkFree)==NULL) entry=nbAlloc(sizeof(NB_Link));
  else nb_LinkFree=entry->next;
  entry->object=grabObject(object);
  entry->next=*set;
  *set=entry;
  return(0);
  }
