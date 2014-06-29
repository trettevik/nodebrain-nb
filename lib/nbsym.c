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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbsym.c 
*
* Title:    Symbolic Substitution Routines
*
* Function:
*
*   This file provides routines for the various forms of symbolic substitution
*   supported by NodeBrain.
*
* Description:
*
*   Reference the NodeBrain User's Guide for a description of symbolic
*   substitution. 
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003/03/15 eat 0.5.2  Split out from main routine.
*
* 2004/08/29 eat 0.6.1  Supporting cell expressions in symbolic substitution
*
*            Previously, symbolic substitution only supported simple terms.
*            We now extend support to any cell expression that resolves to a
*            string or number value.
*
*               ${a}
*               ${5*20}
*               ${a+b}
*               ${mystuff("xyz")}  
*
* 2004/08/29 eat 0.6.1  Dropped support for nested term construction
*
*            No longer support ${{a}{b}} syntax.  This was seldom (if ever)
*            used and conflicted with our desire to support cell expressions
*            in symbolic substitution.  The alternative is to use explicit
*            nested substitution.
*
*               ...   ${{a}{b}}       old syntax
*               $ ... $${${a}${b}}    new syntax
*
* 2004/08/29 eat 0.6.1  Reversed order of substitution and reduction
*      
*            Symbolic substitution is now performed before symbolic reduction
*            so it works properly on input commands.  The other ordered worked
*            fine for rule action commands where symbolic substitution is
*            most common, but didn't make sense for input commands.
*
*               $ ... $${${a}${b}}    ${a} and ${b} substitute before reduction
*
* 2004/08/29 eat 0.6.1  Iterative substitution deliberately avoided
*
*            We have made a decision to avoid automatic repeatative
*            substitution.  This simplifies the use of symbolic substitution
*            while creating commands the use symbolic substitution.  The user
*            is required to use explicit control using the "$" operator.
*
*               assert a="$";
*               define r1 on(...):-echo ${a}{b}
*
*               assert a="$${b}",b=10,c=5;
*               define r0 on(...) a="$${c}";
*               define r1 on(...): $ -echo ${a}
*
*            It is normally preferable to use less clever symbolic substitution
*            when possible.  The following is prefered over the previous examples.
*
*               define r1 on(...):$ -echo ${b}
*
*               assert a==b,b=10,c=5;
*               define r0 on(...) a==c;
*               define r1 on(...):-echo ${a}
*
* 2005/10/03 eat 0.6.3  "??" substitures for Unknown values
* 2006/10/28 eat 0.6.6  Removed [] experiment from nbSymCmd().
* 2012/01/26 dtl 0.8.7  Checker updates
* 2012-10-16 eat 0.8.12 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>

/*
*  Symbolic cell substitution
*    source: <cell>}
*    target: gets value of cell
*    close:  provides expected cell terminator 
*            may be any character that terminates a cell expression
*            used only to check where nbParseCell stops
*            specify '?' if you want to check it yourself
*            
*/
char *nbSymCell(nbCELL context,char **target,char *targetend,char *source,char close){
  struct NB_OBJECT *object,*cell;
  char tmpstr[256],*cursor;
  int n;
  if(trace) outMsg(0,'T',"nbSymCell called [%s].",source);
  if((cell=nbParseCell((NB_Term *)context,&source,0))==NULL) return(NULL);
  grabObject(cell);
  if(*source!=close && close!='?'){
    outMsg(0,'E',"expecting '%c' at end of symbolic cell expression at \"%s\"",close,source);
    dropObject(cell);
    return(NULL);
    }
  source++;
  object=nbCellCompute_((NB_Cell *)cell);
  dropObject(cell);
  if(object->type==realType){
    snprintf(tmpstr,sizeof(tmpstr),"%.10g",((struct REAL *)object)->value); //2012-01-16 dtl used snprintf
    cursor=tmpstr;
    }
  else if(object->type==strType)
    cursor=((struct STRING *)object)->value;
  //else if(object->type==nb_UnknownType) cursor="??";
  else if(object->type==nb_UnknownType) cursor="?";
  else{
    outMsg(0,'E',"substitution value object type not supported.");
    dropObject(object);
    return(NULL);
    }
  if(trace) outMsg(0,'T',"substitution value=[%s].",cursor);
  if(targetend < (*target+(n=strlen(cursor)))){
    outMsg(0,'L',"buffer size insufficient for substitution value");
    dropObject(object);
    return(NULL);
    }
  else strncpy(*target,cursor,n+1); //2012-01-16 dtl:moved into if block,used strncpy,cp include 0 byte
  //*target=strchr(*target,0);      // 2012-10-16 eat - checker uncheck return code false positive
  *target=*target+strlen(*target); 
  dropObject(object);
  return(source);
  }
  
/*
*  Symbolic "reduction" - $${ is replaced by ${
*     Note: We assume symbolic substitution only preserved ${
*           if preceded by another $, so we actually replace
*           ${ with {.
*
*     The sym and open parameters are used to replace '$' and
*     '{' with other symbols if desired.
*/  
void nbSymReduce(char *source,char *target,char sym,char open){
  outMsg(0,'T',"nbSymReduce called");
  while(*source==' ') source++;
  while(*source!=0){
    if(*source==sym && *(source+1)==open) source++;
    *target=*source;
    target++;
    source++;
    }
  *target=0;
  }
            
/*
*  Symbolic substitution
*
*    Returns address of buffer containing modified command
*/
char *nb_symBuf1; // initialized to NB_BUFSIZE for symbolic substitution 
char *nb_symBuf2; // initialized to NB_BUFSIZE for symbolic substitution 

char *nbSymCmd(nbCELL context,char *source,char *style){
  char sym=*style;
  char open=*(style+1);
  char close=*(style+2);
  char *targetbuf=nb_symBuf1,*target,*targetend;

  //outMsg(0,'T',"nbSymCmd called: %s",source);
  if(source>=targetbuf && source<targetbuf+NB_BUFSIZE) targetbuf=nb_symBuf2; // switch if source is in our buffer
  while(*source==sym && *(source+1)==' '){
    //outMsg(0,'T',"nbSymCmd: style=%s: %s",style,source);
    targetend=targetbuf+NB_BUFSIZE-1;
    target=targetbuf;
    source+=2;
    //outMsg(0,'T',"nbSymCmd: substitution request");
    while(*source==' ') source++;  // ignore leading blanks
    // 2008-05-26 eat 0.7.0 - stopped looking for '\n' as it should already be removed
    while(*source!=0){
      if(target>targetend){
        outMsg(0,'L',"Symbolic substitution exceeded buffer size");
        return(NULL);
        }
      if(*source==sym){
        //outMsg(0,'T',"nbSymCmd: found sym");
        if(*(source+1)==open){
          //outMsg(0,'T',"nbSymCmd: found open");
          if((source=nbSymCell(context,&target,targetend,source+2,close))==NULL) return(NULL);
          }
        else if(*(source+1)==sym){
          //outMsg(0,'T',"nbSymCmd: found double sym");
          while(*(source+1)==sym){
            if(target>targetend){
              outMsg(0,'L',"Symbolic substitution exceeded buffer size");
              return(NULL);
              }
            *target=*source;
            target++;
            source++;
            }
          if(*(source+1)==open) source++;
          *target=*source;
          target++;
          source++;
          }
        else{
         *target=*source;
          target++;
          source++;
          }
        }
      else{
        *target=*source;
        target++;
        source++;
        }
      }
    *target=0;
    if(symbolicTrace) outPut("%c %s\n",sym,targetbuf);
    source=targetbuf;
    if(targetbuf==nb_symBuf1) targetbuf=nb_symBuf2;
    else targetbuf=nb_symBuf1;
    }
  //outMsg(0,'T',"nbSymCmd: returning: %s\n-----",source);
  return(source);  
  }

/*
*  Symbolic substitution for source text
*/
char *nbSymSource(nbCELL context,char *source){
  while(*source==' ') source++;
  if(*source=='%' && *(source+1)==' ') return(nbSymCmd(context,source,"%{}"));
  else return(source);
  }
