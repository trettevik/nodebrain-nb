/*
* Copyright (C) 2011 The Boeing Company
*                    Ed Trettevik <eat@nodebrain.org>
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
* File:     nbtext.c 
*
* Title:    Text Object Methods
*
* Function:
*
*   This file provides methods for nodebrain TEXT objects that are defined
*   by a null terminated array of characters.  The TEXT type extends the
*   OBJECT type defined in nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initText();
*   NB_Text *nbTextLoad(char *filename);
*   void printText();
*   char *nbCellGetText(nbCELL context,nbCELL textCell);
* 
* Description
*
*   The text object type is different from the string object type in many
*   ways because it is not initially intended as an object for use by the
*   interpreter.  It is only provided for the convenience of NodeBrain
*   modules.  Text can be defined within the NodeBrain glossary of terms
*   and displayed.  It can not be compared or matched using regular
*   expressions (yet).
*
*   However, a module may use a text object however it wants.  Examples
*   might include an HTML page or a form letter for email.
*
*     define form1 text filename;
*
*     assert useform=form1;
*
*   Like other nodebrain objects, text objects may be grabbed and dropped.

*     mytext=grabObject(nbTextLoad("filename"));
*
*   When a reference to a text object is no longer needed, release the
*   reservation by calling dropObject().
*
*     dropObject(NB_Text *mytext);
*
*   When all references to a text object are dropped, the memory assigned to the
*   object may be reused.
*  
*=============================================================================
* History:
*
*   This object type was inspired by work Cliff Bynum did on an application
*   called Bingo.  The application needed to send form letters and the forms
*   were managed as files, but loaded into memory for quick reference.  Those
*   forms are now NodeBrain text objects.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2011-11-05 Ed Trettevik (original prototype version introduced in 0.8.6)
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *textType;

/**********************************************************************
* Object Management Methods
**********************************************************************/
void printText(NB_Text *text){
  outPut("text\n%s",text->value);
  }

void destroyText(NB_Text *text){
  nbFree(text,sizeof(NB_Text)+strlen(text->value));
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void initText(NB_Stem *stem){
  textType=newType(stem,"text",NULL,0,printText,destroyText);
  textType->apicelltype=NB_TYPE_TEXT;
  }

/*
*  Load text object from file
*/
NB_Text *nbTextLoad(char *fileName){
  NB_Text *text;
  FILE *fp;
  long len;
  short size;
  char *buf;
  fp=fopen(fileName,"r");
  if(fp==NULL){
    outMsg(0,'E', "Can't open text file %s",fileName);
    return(NULL);
    }
  outMsg(0,'T', "input file %s opened",fileName);
  if(fseek(fp,0,SEEK_END)!=0){ 
    outMsg(0,'E', "fseek end of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    return(NULL);
    }
  len=ftell(fp);
  if(fseek(fp,0,SEEK_SET)!=0) { 
    outMsg(0,'E', "fseek begin of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    return(NULL);
    }
  if(len>0x8000-sizeof(NB_Text)){
    outMsg(0,'E', "fseek begin of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    return(NULL);
    }
  size=sizeof(NB_Text)+len;
  text=(NB_Text *)newObject(textType,NULL,size);
  buf=text->value;
  if(fread(buf,len,1,fp)<1){
    outMsg(0,'E', "fread of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    nbFree(text,size);
    return(NULL);
    }
  fclose(fp);
  *(buf+len)=0;
  return(text);
  }

NB_Text *nbTextCreate(char *textStr){
  NB_Text *text;
  short size;

  size=sizeof(NB_Text)+strlen(textStr);
  text=(NB_Text *)newObject(textType,NULL,size);
  strncpy(text->value,textStr,strlen(textStr)+1); 
  return(text);
  }
