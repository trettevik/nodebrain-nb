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
*==============================================================================
*/
#include "nbi.h"

int parseTrace=0;        /* debugging trace flag for parsing routines */

unsigned char nb_CharClass[256];  // see nbparse.h for definitions

void nbParseInit(void){
  unsigned int i;
  char *numChar="0123456789";
  char *alphaChar="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char *leadingChar="_.@%$";
  char *relationChar="=<>";
  char *comboChar="&?[|";      // single character operators that can start combo operators
  char *soloChar="^(,{+-*/`";  // single character operators - consumed
  char *delimChar="])}:;";     // cell delimiter - not consumed 

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
  }

/*
*  The strtok_r function was not found on the Sequent machine used to
*  compile this program for the Sequent platform.  This routine is a
*  work around.  It only supports a single delimiter character.
*/
char *my_strtok_r(char *cursor,char *delim,char **newcur){
  char *mycursor;
  while(*cursor==*delim){
    cursor++;
    }
  if(*cursor==0){
    *newcur=NULL;
    return(NULL);
    }
  mycursor=strchr(cursor,*delim);
  if(mycursor!=NULL){
    *mycursor=0;
    mycursor++;
    *newcur=mycursor;
    }
  else *newcur=strchr(cursor,0);
  return(cursor);
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
int nbParseTerm(char **termP,char **cursorP){
  char *term=*termP,*cursor=*cursorP;
  int len;
  
  while(*cursor=='.'){  /* accept leading periods on a term qualifier */
    *term=*cursor;
    term++;
    cursor++;
    }      
  if(*cursor=='_'){     // accept leading underscore
    *term=*cursor;
    term++;
    cursor++;
    }
  if(*cursor=='\''){
    *term=*cursor;
    term++;
    cursor++;
    while(*cursor!='\'' && *cursor!=0 && *cursor!='\n'){
      *term=*cursor;
      term++;
      cursor++;
      }
    if(*cursor!='\''){
      outMsg(0,'E',"Qualifier with unbalanced quotes at: %s",*cursorP);
      **termP=0;
      return(0);
      }
    *term=*cursor;
    term++;
    cursor++;
    }
  // 2005/05/14 eat 0.6.3  This was cleaned up to not accept special symbols
  //else if(isAlpha(*cursor)){
  else if(NB_ISALPHA((int)*cursor)){
    *term=*cursor;
    term++;
    cursor++;
    //while(isAlphaNumeric((int)*cursor)){
    while(NB_ISALPHANUMERIC((int)*cursor)){
      *term=*cursor;
      term++;
      cursor++;
      }
    }
  *term=0;
  len=term-*termP;
  *termP=term;
  *cursorP=cursor;
  return((int)(len));  
  } 

/*
*  This is a relaxed term parser that allows a term to start with some
*  special characters and consumes a trailing period.  This is used to step
*  through an identifier one term at a time.  The symbol parser should ensure
*  that the special symbols will not prefix any subordinate term.
*
*  We need to review the use of these special prefixes within term glossaries.
*/
char *getQualifier(char *qCursor,char *sCursor){
  if(*sCursor=='@' || *sCursor=='$' || *sCursor=='%'){
    *qCursor=*sCursor;
    qCursor++;
    sCursor++;
    // Give special attention to @. or $. or %.
    // In these cases just return the single symbol
    if(*sCursor=='.') *qCursor=0;
    else nbParseTerm(&qCursor,&sCursor);
    }
  else if(nbParseTerm(&qCursor,&sCursor)==0) return(NULL);
  if(*sCursor=='.') sCursor++; /* step over one trailing period */  
  return(sCursor);  
  }

/*
*  Parse time condition symbol - just looking for balanced brackets
*/
void nbParseTimeSymbol(char *symid,char **identP,char **sourceP){
  int paren;
  char *symbol=*identP,*cursor=*sourceP;
  if(*cursor=='{'){   /* handle schedule plans */
    *symbol=*cursor; symbol++; cursor++;
    paren=1;
    while(paren>0 && *cursor!=0){
      if(*cursor=='{') paren++;
      else if(*cursor=='}') paren--;
      *symbol=*cursor; symbol++; cursor++;
      }
    if(paren>0){
      // 2010-09-17 eat - changed **sourceP to *sourceP to fix crash on unbalanced parens
      outMsg(0,'E',"Unbalanced braces {} in time condition \"%s\".",*sourceP);
      *symid='.';
      }
    }
  else if(*cursor=='('){
    cursor++;
    paren=1;
    while(paren>0 && *cursor!=0){
      if(*cursor=='(') paren++;
      else if(*cursor==')') paren--;
      *symbol=*cursor; symbol++; cursor++;
      }
    if(paren>0){
      // 2010-09-17 eat - changed **sourceP to *sourceP to fix crash on unbalanced parens
      outMsg(0,'E',"Unbalanced parentheses in time condition \"%s\".",*sourceP);
      *symid='.';
      }
    else symbol--;  /* drop trailing parenthesis */
    }
  *identP=symbol;
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
* I  T   ~^1()      ~T() delay true condition      0.7.3
* I  F   ~^0()      ~F() delay false condition     0.7.3
* I  U   ~^?()      ~U() delay unknown condition   0.7.3
*
* S  !   !               Not operator 
* S  ?   ?               Unknown operator or value 0.6.0  use (?) to force it to be a value
* S  1   !!              True operator             0.4.1
* S  k   !?              Known operator            0.4.1
* S  w   []              closed world operator     0.4.1
*
* I  A   &&              lazy and                  0.7.3
* B  &   and
* I  a   !&,nand         nand                      0.4.1
* I  O   ||              lazy or                   0.7.3
* B  |   or
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
* 2005/05/14 eat 0.6.3  leading symbols for terms are handled differently now
* 2006/01/06 eat 0.6.4  recognize null delimiter (0) as term delimiter after "."
* 2008-11-11 eat 0.7.3  Split infix operators out to nbParseSymbolInfix()
*/
char nbParseSymbol(char *symbol,char **source){
  char *cursor,*symsave,symid;
  //char *cur;
  //int paren;

  if(parseTrace) outMsg(0,'T',"nbParseSymbol called [%s].",*source);
  cursor=*source;
  symsave=symbol;
  *symbol=0;
  while(*cursor==' ') cursor++;
  switch(nb_CharClass[(int)*cursor]){
    case NB_CHAR_NUMBER: // handle numbers - integer and real 
      while(NB_ISNUMERIC((int)*cursor)){
        *symbol=*cursor;
        symbol++;
        cursor++;
        }
      symid='i';
      if(*cursor=='.'){
        *symbol=*cursor;
        symbol++;
        cursor++;
        while(NB_ISNUMERIC((int)*cursor)){
          *symbol=*cursor;
          symbol++;
          cursor++;
          }
        symid='r';
        }
      if(*cursor=='e' && (*(cursor+1)=='+' || *(cursor+1)=='-') && NB_ISNUMERIC((int)*(cursor+2))){
        *symbol=*cursor; symbol++; cursor++;
        *symbol=*cursor; symbol++; cursor++;
        *symbol=*cursor; symbol++; cursor++;
        while(NB_ISNUMERIC((int)*cursor)){
          *symbol=*cursor;
          symbol++;
          cursor++;
          }
        symid='r';
        }
      break;
    case NB_CHAR_ALPHA:      // handle words and identifiers including single quoted strings
    case NB_CHAR_TERMQUOTE:
      //outMsg(0,'T',"NB_CHAR_ALPHA or NB_CHAR_TERMQUOTE");
      if(!nbParseTerm(&symbol,&cursor)) return('.');
      symid='t';
      if(*cursor=='.'){
        while(*cursor=='.'){
          *symbol=*cursor;
          symbol++;
          cursor++;
          //if(*cursor!=0 && *cursor!=' ' && !nbParseTerm(&symbol,&cursor)) return('.');
          if(!nbParseTerm(&symbol,&cursor)) symbol--;
          }
        // this seems problematic because a term can start with a period
        if(*symbol=='.') cursor--; // 2006-01-06 eat 0.6.4 - backup when term ends with period
        }
      else if(strcmp(symsave,"not")==0) symid='!';
      break;
    case NB_CHAR_LEADING:     // leading symbol for special identifiers
      //outMsg(0,'T',"NB_CHAR_LEADING");
      if(*cursor=='$' && *(cursor+1)=='('){
        symid='$';
        strncpy(symbol,cursor,2);
        symbol+=2;
        cursor+=2;
        }
      else if(*cursor=='%' && *(cursor+1)=='('){
        symid='%';
        strncpy(symbol,cursor,2);
        symbol+=2;
        cursor+=2;
        }
      else{
        *symbol=*cursor;
        symbol++;
        cursor++;
        // 2009-10-28 eat - included support for real numbers starting with "." (e.g. .35)
        if(*symsave=='.' && NB_ISNUMERIC((int)*cursor)){
          symid='r';
          while(NB_ISNUMERIC((int)*cursor)){
            *symbol=*cursor;
            symbol++;
            cursor++;
            }
          if(*cursor=='e' && (*(cursor+1)=='+' || *(cursor+1)=='-') && NB_ISNUMERIC((int)*(cursor+2))){
            *symbol=*cursor; symbol++; cursor++;
            *symbol=*cursor; symbol++; cursor++;
            *symbol=*cursor; symbol++; cursor++;
            while(NB_ISNUMERIC((int)*cursor)){
              *symbol=*cursor;
              symbol++;
              cursor++;
              }
            }
          }
        else{
          symid='t';
          if(nbParseTerm(&symbol,&cursor)){
            while(*cursor=='.' && nbParseTerm(&symbol,&cursor));
            }
          // 2009-10-28 eat - included test on symsave to not treat single period as a trailing period
          if(symbol>symsave+1 && *(symbol-1)=='.'){  // step back from trailing period
            symbol--;
            cursor--;
            }
          }
        }
      break;
    case NB_CHAR_QUOTE:    // handle string literals
      cursor++;
      while(*cursor!='\"' && *cursor!=0 && *cursor!='\n'){
        *symbol=*cursor;
        symbol++;
        cursor++;
        }
      if(*cursor=='\"'){
        *symbol=0;
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
        *symbol=*cursor; symbol++; cursor++;
        if(*cursor=='='){
          *symbol=*cursor; symbol++; cursor++; 
          }
        }
      else symid='.';
      break;
    case NB_CHAR_TILDE:
      symid=*cursor;
      cursor++;
      /* handle change */
      if(*cursor=='='){
        symid='c';
        *symbol=*cursor; symbol++; cursor++;
        break;
        }
      nbParseTimeSymbol(&symid,&symbol,&cursor);
      break;
    case NB_CHAR_NOT:     // handle operators starting with "!"
      symid='!'; 
      *symbol=*cursor; symbol++; cursor++;
      if(*cursor=='!') symid='1';                // !!
      else if(*cursor=='?') symid='k';           // !?
      if(symid!='!'){
        *symbol=*cursor; symbol++; cursor++;
        }
      break;
    case NB_CHAR_COMBO:    // handle combination operators and related single char operators
      /* handle unknown value */
      if(*cursor=='?' && *(cursor+1)=='?'){
          symid='u';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          outMsg(0,'W',"Replace deprecated '?\?' with '?' or '(?)'."); // escape to avoid a trigraph warning
          break;
          }
      // consider getting rid of the closed world operator syntax
      //    we can reduce a?0 to the same cell type as []a
      //    not using [ seem like a good idea since we may want that
      /* handle closed world prefix operator */
      else if(*cursor=='[' && *(cursor+1)==']'){
        symid='w';
        *symbol=*cursor; symbol++; cursor++;
        *symbol=*cursor; symbol++; cursor++;
        break;
        }
      // fall through to handle single character when combo not found
    case NB_CHAR_SOLO:    // handle characters not combining with others
      symid=*cursor;
      *symbol=*cursor; symbol++; cursor++;
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
  *symbol=0;
  if(parseTrace) outMsg(0,'T',"nbParseSymbol returning ['%c',\"%s\"] [%s].",symid,symsave,cursor);
  return(symid);
  }

/*
*  Symbol Parser (Infix)
*
*    See comments above with nbParseSymbol
*/
char nbParseSymbolInfix(char *symbol,char **source){
  char *cursor,*symsave,symid;
  //char *cur;
  //int paren;

  if(parseTrace) outMsg(0,'T',"nbParseSymbolInfix called [%s].",*source);
  cursor=*source;
  symsave=symbol;
  *symbol=0;
  while(*cursor==' ') cursor++;
  switch(nb_CharClass[(int)*cursor]){
    case NB_CHAR_ALPHA:      // handle words and identifiers including single quoted strings
      while(NB_ISALPHA((int)*cursor)){
        *symbol=*cursor;
        symbol++;
        cursor++;
        }
      *symbol=0;
      if(strcmp(symsave,"and")==0) symid='&';
      else if(strcmp(symsave,"nand")==0) symid='a';
      else if(strcmp(symsave,"or")==0) symid='|';
      else if(strcmp(symsave,"nor")==0) symid='o';
      else if(strcmp(symsave,"xor")==0) symid='x';
      else symid='.';
      break;
    case NB_CHAR_RELATION:   // handle relational operators
      while(*cursor!=0 && strchr("=<>",*cursor)!=NULL){
        *symbol=*cursor;
        symbol++;
        cursor++;
        }
      symid='=';
      break;
    case NB_CHAR_TILDE:
      /* handle regular expression match */
      if(*(cursor+1)=='~'){
        symid='m';
        *symbol=*cursor; symbol++; cursor++;
        *symbol=*cursor; symbol++; cursor++;
        }
      /* handle time condition (prefix) and match (infix) conditions */
      else{
        symid=*cursor;
        cursor++;
        if(*cursor=='^'){  // new syntax time delay: ~^1(...) ~^0(...) ~^?(...)
          cursor++;
          if(*cursor=='1') symid='T';
          else if(*cursor=='0') symid='F';
          else if(*cursor=='?') symid='U';
          else symid='.';
          cursor++;
          nbParseTimeSymbol(&symid,&symbol,&cursor);
          }
        // support deprecated syntax
        else if(*cursor=='T' || *cursor=='F' || *cursor=='U'){
          symid=*cursor;
          cursor++;
          nbParseTimeSymbol(&symid,&symbol,&cursor);
          }
        else symid='m';
        }
      break;
    case NB_CHAR_NOT:     // handle operators starting with "!"
      symid='!';
      *symbol=*cursor; symbol++; cursor++;
      if(*cursor=='&') symid='a';
      else if(*cursor=='|') symid='o';
      if(symid!='!'){
        *symbol=*cursor; symbol++; cursor++;
        }
      break;
    case NB_CHAR_COMBO:    // handle combination operators and related single char operators
      if(*cursor=='|'){
        if(*(cursor+1)=='|'){  // lazy and
          symid='O';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        else if(*(cursor+1)=='~' && *(cursor+2)=='|'){ // |~| or enable monitor
          symid='e';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        else if(*(cursor+1)=='^' && *(cursor+2)=='|'){ // |^| or value capture
          symid='v';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        /* handle exclusive or |!& */
        else if(strncmp(cursor,"|!&",3)==0){
          symid='x';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        }
      else if(*cursor=='&'){
        if(*(cursor+1)=='&'){   // && lazy and
          symid='A';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        else if(*(cursor+1)=='~' && *(cursor+2)=='&'){  // &^& and value capture
          symid='E';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        else if(*(cursor+1)=='^' && *(cursor+2)=='&'){  // &^& and value capture
          symid='V';
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          *symbol=*cursor; symbol++; cursor++;
          break;
          }
        }
      // fall through to handle single character when combo not found
    case NB_CHAR_SOLO:    // handle characters not combining with others
      symid=*cursor;
      *symbol=*cursor; symbol++; cursor++;
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
  if(symid=='.') *symsave=0,cursor=*source;
  else *source=cursor,*symbol=0;
  if(parseTrace) outMsg(0,'T',"nbParseSymbolInfix returning ['%c',\"%s\"] [%s].",symid,symsave,cursor);
  return(symid);
  }


/*
* Parse low level object expressions (objects, terms, calls)
*
*   This is level 7 from nbParseCell's perspective
*/
NB_Object *nbParseObject(NB_Term *context,char **cursor){
  char ident[NB_BUFSIZE],token[1024];
  void *right;
  NB_Object *object,*objhold;
  NB_Term *term;
  char *delim,msg[256],*savecursor=*cursor;
  struct TYPE *type;
  //int not=0;
  char symid;

  if(parseTrace) outMsg(0,'T',"nbParseObject(): called -->%s",*cursor);
  symid=nbParseSymbol(ident,cursor);
  if(parseTrace) outMsg(0,'T',"nbParseObject(): nbParseSymbol returned ['%c',\"%s\"]-->%s",symid,ident,*cursor);

  if(NB_ISCELLDELIM((int)symid)) return(NULL);
  //if(strchr(")}:;",symid)!=NULL) return(NULL);
  if(symid==','){(*cursor)--; return(NULL);}
  switch(symid){
    case '!': // !
      if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      //if((object=nbParseRel(context,cursor))==NULL) return(NB_OBJECT_FALSE);
      if(object==nb_Unknown) return(nb_Unknown);
      if(object==NB_OBJECT_FALSE) return(NB_OBJECT_TRUE);
      if(object->value==object) return(NB_OBJECT_FALSE);
      return((NB_Object *)useCondition(0,condTypeNot,object,nb_Unknown));
    case '1': // !! true
      if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      //if((object=nbParseRel(context,cursor))==NULL) return(NB_OBJECT_TRUE);
      if(object==nb_Unknown || object==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE); // false constant
      if(object->value==object) return(NB_OBJECT_TRUE);  // true constant
      return((NB_Object *)useCondition(0,condTypeTrue,object,nb_Unknown));
    case 'k': // !? known
      //if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if((object=nbParseRel(context,cursor))==NULL) return(nb_Unknown);
      if(object==nb_Unknown) return(NB_OBJECT_FALSE);   // unknown constant
      if(object->value==object) return(NB_OBJECT_TRUE); // known constant
      if(object->type==condTypeUnknown || object->type==condTypeKnown) return(NB_OBJECT_TRUE);
      return((NB_Object *)useCondition(0,condTypeKnown,object,nb_Unknown));
    case '?': // ? unknown
      //if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if((object=nbParseRel(context,cursor))==NULL) return(nb_Unknown);
      if(object==nb_Unknown) return(NB_OBJECT_TRUE);     // we know unknown is unknown
      if(object->value==object) return(NB_OBJECT_FALSE); // other constants are known
      if(object->type==condTypeUnknown || object->type==condTypeKnown) return(NB_OBJECT_FALSE);
      return((NB_Object *)useCondition(0,condTypeUnknown,object,nb_Unknown));
    case 'w': // [] closed world
      if((object=nbParseRel(context,cursor))==NULL) return(NULL);
      if(object==nb_Unknown) return(NB_OBJECT_FALSE);      // unknown if false in closed world
      if(object==NB_OBJECT_FALSE) return(NB_OBJECT_FALSE); // false is false
      if(object->value==object) return(NB_OBJECT_TRUE);     // other constants are true
      return((NB_Object *)useCondition(0,condTypeClosedWorld,object,nb_Unknown));
    case 'c':
      object=nbParseObject(context,cursor);
      if(object==NULL) return(NULL);
      return((NB_Object *)useCondition(0,condTypeChange,object,nb_Unknown));
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
        outMsg(0,'E',"Unbalanced braces at [%s].",*cursor);
        return(NULL);
        }
      *cursor=*cursor+1; // consume right brace 
      /*
      *  Get the value of the object and then let the object go.
      *  The use of enable and disable is a bit inefficient because
      *  it unnecessarily registers objects for alerts and then
      *  unregisters them.  It would be better to set a mode flag so
      *  nbCellEnable only calls the eval method when called by
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
      if(NULL==(object=(NB_Object *)nbRuleParse((nbCELL)context,0,cursor,msg))) outPut("%s\n",msg);
      return(object);
    case '~': // time condition 
      if(parseTrace) outMsg(0,'T',"Calling newSched A [%s].",ident);
      if(NULL==(object=(NB_Object *)newSched((nbCELL)context,symid,ident,&delim,msg,1))){
        outPut("%s\n",msg);
        return(NULL);
        }
      if(parseTrace) outMsg(0,'T',"Schedule structure generated.");
      return((NB_Object *)useCondition(0,condTypeTime,nb_Unknown,object));
    case '-':
    case '+': // number 
      savecursor=*cursor;
      *token=symid;
      symid=nbParseSymbol(ident,cursor); 
      if(symid=='r' || symid=='i'){
        strcpy(token+1,ident);
        return((NB_Object *)parseReal(token));
        }
      *cursor=savecursor;
      if(*token=='+') return(nbParseObject(context,cursor));
      object=nbParseObject(context,cursor);
      if(object!=NULL) return((NB_Object *)useCondition(0,mathTypeInv,nb_Unknown,object));
      *cursor=savecursor;
      return(NULL);
    case 'u': // unknown
      return(nb_Unknown);
    case 'r':
    case 'i': // number
      return((NB_Object *)parseReal(ident));
    case 's': // string literal 
      return((NB_Object *)useString(ident));
    case 't': // term identifier 
      // A single underscore identifies the placeholder cell
      // Perhaps this determination should be moved to nbParseSymbol
      if(*ident=='_' && *(ident+1)==0) return(nb_Placeholder);
  
      if(parseTrace) outMsg(0,'T',"nbParseObject(): parsed term \"%s\"",ident);
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
      /* default to cell term if not define */
      savecursor=*cursor;
      symid=nbParseSymbol(token,cursor);
      if(symid!='('){
        (*cursor)=savecursor;
        // 2006-12-22 eat - when ready, experiment with using nb_Disabled definition for "undefined" terms
        //if(term==NULL) term=nbTermNew(context,ident,nb_Disabled);
        if(term==NULL) term=nbTermNew(context,ident,nb_Unknown);
        return((NB_Object *)term);
        }
      right=parseList(context,cursor);
      symid=nbParseSymbol(token,cursor);
      if(symid!=')'){
        outMsg(0,'E',"Expecting \")\" at end of parameter list.");
        return(NULL);
        }
      (*cursor)++;
      if(strcmp(ident,"_mod")==0){
        type=callTypeMod;
        return((NB_Object *)useCondition(0,type,useReal(0),right));
        }
      if(term==NULL){  /* included logic to lookup builtin function names */
        NB_Link *member;
        struct TYPE *objType;
        for(member=regfun;member!=NULL;member=member->next){
          objType=((struct TYPE *)member->object);
          if(strcmp(objType->name,ident)==0){
            return(objType->construct(objType,((NB_List *)right)->link));
            }
          }
        /* A term with an Unknown definition can be defined later */
        term=nbTermNew(context,ident,nb_Unknown);
        return((NB_Object *)useCondition(0,condTypeNode,term,right));
        }
      else if(term->def->type==nb_NodeType){
        return((NB_Object *)useCondition(0,condTypeNode,term,right));
        }
      else{
        outMsg(0,'E',"Node condition requires term defined as node.");
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
  NB_Object *lobject,*robject;
  char *savecursor;
  struct TYPE *type;
  int not=0;

  if(parseTrace) outMsg(0,'T',"nbParseRel(): called [%s].",*cursor);
  if((lobject=nbParseCell(context,cursor,5))==NULL) return(NULL);
  savecursor=*cursor;
  symid=nbParseSymbolInfix(operator,cursor);
  if(parseTrace) outMsg(0,'T',"nbParseRel(): nbParseSymbol returned ['%c',\"%s\"].",symid,operator);
  if(strchr(")}:;",symid)!=NULL) return(lobject);

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
    if((symid=nbParseSymbol(token,cursor))!='s'){
      outMsg(0,'E',"Expecting string literal regular expression.");
      return(NULL);
      }
    re=newRegexp(token,PCRE_NO_AUTO_CAPTURE);
    /* let newRegexp print error messages for now.  In the future, we
    *  should return a message and offset. */
    if(re==NULL) return(NULL);
    if(parseTrace) outMsg(0,'T',"Encountered regular expression.");
    return((NB_Object *)useCondition(not,type,lobject,re));
    }
  if((robject=nbParseCell(context,cursor,5))==NULL){
    outMsg(0,'E',"Expecting right operand to relational operator at [%s]",cursor);
    /* dropObject(lobject); add this later */
    return(NULL);
    }
  return((NB_Object *)useCondition(not,type,lobject,robject));
  }


/*
* Parse cell expression
*/
NB_Object *nbParseCell(NB_Term *context,char **cursor,int level){
  char operator[256];
  NB_Object *lobject,*robject,*sobject;
  char *delim,msg[256];
  struct TYPE *type;
  char symid,*cursave;

  if(parseTrace) outMsg(0,'T',"nbParseCell(%d): called [%s].",level,*cursor);
  if(level==6) lobject=nbParseObject(context,cursor);
  else if(level==3) lobject=nbParseRel(context,cursor);
  else lobject=nbParseCell(context,cursor,level+1);
  if(lobject==NULL) return(NULL);
  while(1){
    if(parseTrace) outMsg(0,'T',"nbParseCell(%d): calling nbParseSymbolInfix [%s].",level,*cursor);
    cursave=*cursor;
    symid=nbParseSymbolInfix(operator,cursor);
    if(parseTrace) outMsg(0,'T',"nbParseCell(%d): back from nbParseSymbolInfix [%s].",level,*cursor);
    if(strchr(")}:;",symid)!=NULL) return(lobject);
    if(symid=='.'){
      if(level!=0) return(lobject);
      outMsg(0,'E',"Operator not recognized at-->%s",cursave);
      return(NULL);
      }
    /* should , be a terminator like the above symbols and not be consumed? */
    if(symid==','){(*cursor)--; return(lobject);}
    robject=NULL;
    switch(level){
      case 0:
        if(symid=='|') type=condTypeOr;
        else if(symid=='O') type=condTypeLazyOr;
        else if(symid=='o') type=condTypeNor;
        else if(symid=='x') type=condTypeXor;
        else{
          outMsg(0,'T',"Operator \"%s\" not expected.",operator);
          return(NULL);
          }
        if((robject=nbParseCell(context,cursor,0))!=NULL){
          //if(type==condTypeOr || type==condTypeLazyOr)
          //  if((sobject=reduceOr(lobject,robject))!=NULL) lobject=sobject;
          //else lobject=(NB_Object *)useCondition(0,type,lobject,robject);
          if((type==condTypeOr || type==condTypeLazyOr) && (sobject=reduceOr(lobject,robject))!=NULL) lobject=sobject;
          else lobject=(NB_Object *)useCondition(0,type,lobject,robject);
          }
        break;
      case 1:
        if(symid=='&') type=condTypeAnd;
        else if(symid=='A') type=condTypeLazyAnd;
        else if(symid=='a') type=condTypeNand;
        else if(symid=='?') type=condTypeDefault;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,1))!=NULL){
          if(type==condTypeDefault){
            if(lobject==nb_Unknown) lobject=robject;        // use right if left is unknown
            else if(lobject->value==lobject || robject==nb_Unknown); // use left if constant left or unknown right
            else lobject=(NB_Object *)useCondition(0,type,lobject,robject);
            }
          else if((type==condTypeAnd || type==condTypeLazyAnd) && 
            (sobject=reduceAnd(lobject,robject))!=NULL) lobject=sobject;
          else lobject=(NB_Object *)useCondition(0,type,lobject,robject);
          }
        break;
      case 2:
        if(symid=='^') type=condTypeFlipFlop;
        else if(symid=='V') type=condTypeAndCapture;
        else if(symid=='v') type=condTypeOrCapture;
        else if(symid=='E') type=condTypeAndMonitor;
        else if(symid=='e') type=condTypeOrMonitor;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,3))!=NULL) lobject=(NB_Object *)useCondition(0,type,lobject,robject);
        break;
      case 3:
        if(symid=='T') type=condTypeDelayTrue;
        else if(symid=='F') type=condTypeDelayFalse;
        else if(symid=='U') type=condTypeDelayUnknown;
        else{*cursor=cursave; return(lobject);}
        robject=(NB_Object *)newSched((nbCELL)context,symid,operator,&delim,msg,1);
        if(robject==NULL){
          outPut("%s\n",msg);
          return(NULL);
          }
        lobject=(NB_Object *)useCondition(0,type,lobject,robject);
        break;
      /* level 4 is handled by nbParseRel */
      case 5:
        if(symid=='+') type=mathTypeAdd;
        else if(symid=='-') type=mathTypeSub;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseCell(context,cursor,6))!=NULL) lobject=(NB_Object *)useMath(0,type,lobject,robject);
        break;
      case 6:
        if(symid=='*') type=mathTypeMul;
        else if(symid=='/') type=mathTypeDiv;
        else{*cursor=cursave; return(lobject);}
        if((robject=nbParseObject(context,cursor))!=NULL) lobject=(NB_Object *)useMath(0,type,lobject,robject);
        break;
      /* level 7 is handled by nbParseObject */
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
  char *cursor=*curP,ident[256],ident2[256],symid=',';
  NB_Link *member=NULL,*entry,**next;
  NB_Term   *term;
  NB_List   *list=NULL;
  NB_Object *object,*objectL;
  struct TYPE *type;
  char not,unknown;

  if(parseTrace) outMsg(0,'T',"nbParseAssertion() called");
  next=&member;
  while(symid==','){
    *curP=cursor;
    symid=nbParseSymbol(ident,&cursor);
    if(symid=='!'){
      not=1;
      symid=nbParseSymbol(ident,&cursor);
      }
    else not=0;
    if(symid=='?'){
      unknown=1;
      symid=nbParseSymbol(ident,&cursor);
      }
    else unknown=0;
    if(symid=='('){  /* allow for null term */
      *ident=0;
      cursor--;
      }
    else if(symid!='t'){
      outMsg(0,'E',"Expecting term at \"%s\".",*curP);
      return(NULL);
      }
    *curP=cursor;
    if(*cursor=='('){ /* node assertion */
      cursor++;
      list=parseList(cellContext,&cursor);
      symid=nbParseSymbol(ident2,&cursor);
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
        outMsg(0,'E',"Unexpected = with ! or ? operator at \"%s\".",cursor);
        return(NULL);
        }
      cursor++;
      if(*cursor=='.' && *(cursor+1)=='='){
        type=assertTypeRef;
        cursor+=2;
        *curP=cursor;
        symid=nbParseSymbol(ident2,&cursor);
        if(symid!='t'){
          outMsg(0,'E',"What? Expecting term at \"%s\".",*curP);
          return(NULL);
          }
        object=(NB_Object *)nbTermFind(termContext,ident2);
        }
      else{
        /* $x=a+b is an alternative to x==a+b */
        /* this is an experiment---probably not a good idea - don't document */
        if(*ident=='$') type=assertTypeDef;
        else type=assertTypeVal;
        if(*cursor=='='){
          type=assertTypeDef;
          cursor++;
          }
        *curP=cursor;
        object=nbParseCell(cellContext,&cursor,0);
        }
      }
    else if(*cursor==',' || *cursor==':' || *cursor==';' || *cursor==0){
      type=assertTypeVal;
      if(not) object=NB_OBJECT_FALSE;
      else if(unknown) object=nb_Unknown;
      else object=NB_OBJECT_TRUE;
      }
    else{
      outMsg(0,'E',"Expecting '=' ',' or ';' at \"%s\".",*curP);
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
        term=nbTermNew(termContext,ident+1,nb_Unknown);
        }
      else if(*ident=='%'){
        if(*(ident+1)==0){
          outMsg(0,'E',"Expecting term after %");
          return(NULL);
          } 
        term=nbTermNew(symContext,ident+1,nb_Unknown);
        }
      else term=nbTermNew(termContext,ident,nb_Unknown);
      }
    else if((term->def->type->attributes&TYPE_WELDED) && type!=assertTypeRef){
      outMsg(0,'E',"Term \"%s\" is not open to assertion.",ident);
      return(NULL);
      }
    if(list!=NULL){
      objectL=(NB_Object *)useCondition(0,condTypeNode,term,list);
      object=(NB_Object *)useCondition(0,type,objectL,object);
      list=NULL;
      }
    else object=(NB_Object *)useCondition(0,type,term,object);
    if((entry=nb_LinkFree)==NULL) entry=malloc(sizeof(NB_Link));
    else nb_LinkFree=entry->next;
    *next=entry;
    entry->next=NULL;
    next=&(entry->next);
    entry->object=grabObject(object);
    /* may want to enable the object here
    * otherwise we can let nbTermAssign do it
    */
    *curP=cursor;
    symid=nbParseSymbol(ident,&cursor);
    }
  return(member);
  }

// Add an assertion to an assertion list 
//   This will be moved to nbassertion.c
int nbAssertionListAddTermValue(nbCELL context,nbSET *set,nbCELL term,nbCELL cell){
  NB_Link   *entry;
  NB_Object *object;
  object=(NB_Object *)useCondition(0,assertTypeVal,term,cell);
  if((entry=nb_LinkFree)==NULL) entry=malloc(sizeof(NB_Link));
  else nb_LinkFree=entry->next;
  entry->object=grabObject(object);
  entry->next=*set;
  *set=entry;
  return(0);
  }
