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
*   that needed to send form letters and the forms were managed as files,
*   but loaded into memory for quick reference.  Those forms are now NodeBrain
*   text objects.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2011-11-05 Ed Trettevik (original prototype version introduced in 0.8.6)
* 2012-12-18 eat 0.8.13 Checker updates
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
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
  textType=nbObjectType(stem,"text",0,0,printText,destroyText);
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
  fp=fopen(fileName,"r");   // 2013-01-01 eat - VID 5420-0.8.13-1 Intentional
  if(fp==NULL){
    outMsg(0,'E', "Can't open text file %s",fileName);
    return(NULL);
    }
  outMsg(0,'T', "input file %s opened",fileName);
  if(fseek(fp,0,SEEK_END)!=0){ 
    outMsg(0,'E', "fseek end of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    fclose(fp);
    return(NULL);
    }
  len=ftell(fp);
  if(fseek(fp,0,SEEK_SET)!=0) { 
    outMsg(0,'E', "fseek begin of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    fclose(fp);
    return(NULL);
    }
  if(len>0x8000-sizeof(NB_Text)){
    outMsg(0,'E', "fseek begin of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    fclose(fp);
    return(NULL);
    }
  size=sizeof(NB_Text)+len;
  text=(NB_Text *)newObject(textType,NULL,size);
  buf=text->value;
  if(fread(buf,len,1,fp)<1){
    outMsg(0,'E', "fread of %s failed, errno= %d (%s)",fileName,errno,strerror(errno));
    nbFree(text,size);
    fclose(fp);       // 2012-12-18 eat - CID 751609
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
  strcpy(text->value,textStr); // 2013-01-01 eat - VID 5421-0.8.13-01 FP - simplified by replacing strncpy with strcpy 
  return(text);
  }
