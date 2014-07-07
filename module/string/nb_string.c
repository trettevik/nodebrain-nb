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
* File:     nb_string.c
*
* Title:    String Manipulation Node Module
*
* Function:
*
#   This file is a node module supporting string manipulation.
* 
* Syntax:
*
*   chrsub(<string>,"xy")             # character x replaces y
*   utc(<timestring>,<formatstring>)  # converts time to UTC decimal string  
*                                       (see C strptime function)
*
* Description:
*
*   This is effectively a function library because the skills only provide
*   an evaluation method.  There is no intent to provide a complete and
*   robust set of functions.  We are not trying to make NodeBrain a general
*   purpose language.  Serious string manipulation tasks should be handled
*   using general purpose scripting languages.  We only plan to include
*   functions here that are frequently needed by event collecting modules.
*
*   We are more concerned about helping a collector convert values into a
*   form for easy processing within NodeBrain than we are with formatting
*   output for use by other systems.  However, a little bit of string
*   manipulation capability can help in both cases.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2009-09-29 Ed Trettevik - initial prototype
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
# 2011-02-01 eat 0.8.0  included utc function
*=============================================================================
*/
#define _XOPEN_SOURCE
#include "config.h"
#include <nb/nb.h>
#include <ctype.h>

#if defined(_WINDOWS)
BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved){
	switch(fdwReason){
	  case DLL_PROCESS_ATTACH: break;
	  case DLL_THREAD_ATTACH: break;
	  case DLL_THREAD_DETACH: break;
	  case DLL_PROCESS_DETACH: break;
	}
  return(TRUE);
  }
#endif

/*
*========================================================================
* chrsub() skill example:
*
* Description:
*
*   The chrsub skill substitutes characters within a string.
*
*       define chrsub node string.chrsub;
*       define x cell chrsub(a,";,");
*       define r1 on(x="abc,def,xyz");
*
*       assert a="abc,def;xyz";
*
*       Value of x is "abc,def,xyz" and r1 fires.
*
*   This skill provides only an Evaluate method, so it is not capable
*   of commands and assertions.
*
*========================================================================
*/

/* Evaluation Method */

nbCELL chrsubEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  nbCELL cell,argCell1,argCell2;
  nbSET argSet;
  int i,type,len,sublen;
  char *strIn,*strSub,*strChr,*strBuf;
  //char strBuf[NB_BUFSIZE];

  argSet=nbListOpen(context,arglist);
  if(argSet==NULL) return(NB_CELL_UNKNOWN);
  argCell1=nbListGetCellValue(context,&argSet);
  if(argCell1==NULL) return(argCell1);
  type=nbCellGetType(context,argCell1);
  if(type!=NB_TYPE_STRING){
    nbCellDrop(context,argCell1);
    return(argCell1);
    }
  strIn=nbCellGetString(context,argCell1);
  nbCellDrop(context,argCell1);
  argCell2=nbListGetCellValue(context,&argSet);
  if(argCell2==NULL) return(argCell1);
  type=nbCellGetType(context,argCell2);
  if(type!=NB_TYPE_STRING){
    nbCellDrop(context,argCell2);
    nbCellDrop(context,argCell1);
    return(argCell1);
    }
  strSub=nbCellGetString(context,argCell2);
  len=strlen(strIn)+1;
  strBuf=(char *)nbAlloc(len);
  strncpy(strBuf,strIn,len);
  sublen=strlen(strSub);
  for(i=0;i<sublen-1;i+=2){
    strIn=strBuf;
    while((strChr=strchr(strIn,strSub[i]))!=NULL){
      *strChr=strSub[i+1];
      strIn=strChr;
      }
    }
  nbCellDrop(context,argCell1);
  nbCellDrop(context,argCell2);
  cell=nbCellCreateString(context,strBuf);
  nbFree(strBuf,len);
  return(cell);
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern
#endif
void *chrsubBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,chrsubEvaluate);
  return(NULL);
  }

/*
*========================================================================
* utc() skill example:
*
* Description:
*
*   The utc skill converts a time string into a UTC decimal string.
*
*      utc("YYYY-MM-DD hh:mm:ss -hhmm")   returns UTC time string
*
*   A "/" is accepted in place of "-" in the date and "." is accepted
*   in place of ":" in the time.  A partial time string is accepted
*   by defaulting the missing tail as follows.
*
*       1900-01-01 00:00:00 +0000
*   
*   Leading zeros for each component number may be dropped, except for
*   the time offset from GMT,
*
*   A string that does not conform to the expected format is passed
*   through to the result unmodified.
*
*   Example:
*
*       define utc node string.utc;
*       define x cell utc("2011-02-01 10:14:21");
*       define r2 if(time>x)...
*       $ alert time=${timestring},y=...;
*
*   This skill provides only an Evaluate method, so it is not capable
*   of commands and assertions.
*
*========================================================================
*/

/* Evaluation Method */

nbCELL utcEvaluate(nbCELL context,void *skillHandle,void *knowledgeHandle,nbCELL arglist){
  nbCELL argCell1,argCell2;
  nbSET argSet;
  int type;
  char *strIn,*strFormat;
  char strBuf[16];
  struct tm tm;
  time_t utime;
  char *cursor;
  int h,m;
  char sign;

  argSet=nbListOpen(context,arglist);
  if(argSet==NULL) return(NB_CELL_UNKNOWN);
  argCell1=nbListGetCellValue(context,&argSet);
  if(argCell1==NULL) return(argCell1);
  type=nbCellGetType(context,argCell1);
  if(type!=NB_TYPE_STRING){
    nbCellDrop(context,argCell1);
    return(argCell1);
    }
  strIn=nbCellGetString(context,argCell1);
  argCell2=nbListGetCellValue(context,&argSet);
  if(argCell2==NULL) strFormat="%Y-%m-%d %H:%M:%S";
  else{
    type=nbCellGetType(context,argCell2);
    if(type!=NB_TYPE_STRING){
      nbCellDrop(context,argCell2);
      nbCellDrop(context,argCell1);
      return(argCell1);
      }
    strFormat=nbCellGetString(context,argCell2);
    }
  // initialize tm here
  tm.tm_sec=0;
  tm.tm_min=0;
  tm.tm_hour=0;
  tm.tm_mday=1;
  tm.tm_mon=1;
  tm.tm_year=0;
  tm.tm_wday=0;
  tm.tm_yday=0;
  tm.tm_isdst=0;
  // convert to broken down format
  cursor=strIn;
  while(*cursor==' ') cursor++;
  //if!isdigit(*cursor)) 
  //nbLogMsg(context,0,'T',"strIn=%s strFormat=%s",strIn,strFormat);
  cursor=strptime(strIn,strFormat,&tm);
  //nbLogMsg(context,0,'T',"cursor:%s",cursor);
  if(cursor){
    while(*cursor==' ') cursor++;
    if(*cursor=='+' || *cursor=='-'){
      sign=*cursor;
      cursor++;
      if(isdigit(*cursor) && isdigit(*(cursor+1)) && isdigit(*(cursor+2)) && isdigit(*(cursor+3))){
        h=(*cursor-'0')*10+(*(cursor+1)-'0');
        m=(*(cursor+2)-'0')*10+(*(cursor+3)-'0');
        if(sign=='+') h=-h,m=-m;
        tm.tm_hour+=h;
        tm.tm_min+=m;
        }
      }
    }
  utime=mktime(&tm);
  //nbLogMsg(context,0,'T',"utime=%d",utime);
  // adjust to GMT because mktime uses local time
  utime+=utime-mktime(gmtime(&utime));
  sprintf(strBuf,"%d",(int)utime);
  nbCellDrop(context,argCell1);
  nbCellDrop(context,argCell2);
  return(nbCellCreateString(context,strBuf));
  }

/* Skill Initialization Method */

#if defined(_WINDOWS)
_declspec (dllexport)
#else
extern
#endif
void *utcBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_EVALUATE,utcEvaluate);
  return(NULL);
  }

