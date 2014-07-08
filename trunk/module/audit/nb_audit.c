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
*
* Name:   nb_audit.c
*
* Title:  NodeBrain Audit Skill Module 
*
* Function:
*
*   This program is a NodeBrain skill module that supports log file
*   audits.
*   
* Synopsis:
*
*   define <term> node audit("<logfile>","<translator>",<schedule>);
*
* Description:
*
*   A log file is any text file that grows over time.  Normally each line
*   identifies an event, although this is not a requirement.
*
* History:
*
*   This module replaces the obselete LOG listener.
*
*   define <term> listener type="LOG",translator=<translatorTerm>,schedule==<schedule>;
*
*=====================================================================
* NOTE: 
*   1) We need to modify the LOG listener code as we transfer it to this
*      skill module to provide an identity for calls to nbCmdSid() 
*   2) We will drop functionality that can be handled in other ways
*      a) pipe option should be replaced with node that can write to
*         a file
*      b) command to format file can be replaced by a servant script
*         that writes to a log file or pipe.
*   3) We should consider the possibility of applying a translator to
*      any input source.  For example, a fifo skill module could run
*      lines through a translator.
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2007/06/25 Ed Trettevik - original skill module prototype version
* 2007/06/25 eat 0.6.8  Structured skill module around old LOG listener code
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>

//=============================================================================
 
struct NB_MOD_AUDIT{
  FILE   *file;                // audit file
  long   pos;                  // current position within file
  nbCELL fileNameCell;         // cell containing file name
  char   *fileName;            // file name
  nbCELL translatorNameCell;   // cell containing translator name
  char   *translatorName;      // translator name
  nbCELL translatorCell;       // translator cell
  nbCELL scheduleCell;         // schedule for checking file for new lines
  nbCELL synapseCell;          // synapse for monitoring schedule
  unsigned char trace;         // trace option
  };

typedef struct NB_MOD_AUDIT nbAudit;



//==================================================================================
// Functions used by skill methods
//==================================================================================

int auditDisable(nbCELL context,void *skillHandle,nbAudit *audit);

// Check for new lines in the file
//   This function is scheduled by auditEnable() using nbSynapseOpen()

void auditAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  nbAudit *audit=(nbAudit *)nodeHandle;
  nbCELL value=nbCellGetValue(context,cell);
  char logbuf[2048],*cursor;
  long endloc;

  if(value!=NB_CELL_TRUE) return;  // only act when schedule toggles to true

  if((audit->file=fopen(audit->fileName,"r"))==NULL){
    nbLogMsg(context,0,'E',"Log file \"%s\" not found - disabling node.",audit->fileName);
    auditDisable(context,skillHandle,audit);
    return;
    }
  if((fseek(audit->file,0,SEEK_END))!=0){
    nbLogMsg(context,0,'E',"Log file \"%s\" fseek failed errno=%d - disabling node.",audit->fileName,errno);
    auditDisable(context,skillHandle,audit);
    return;
    }
  endloc=ftell(audit->file);
  if(endloc==audit->pos){
    if(audit->trace) nbLogMsg(context,0,'T',"File \"%s\" has not grown.",audit->fileName);
    fclose(audit->file);
    audit->file=NULL;
    return;
    }
  if(endloc<audit->pos){
    nbLogMsg(context,0,'I',"File \"%s\" has shrunk from %d to %d, starting at beginning.",audit->fileName,audit->pos,endloc);
    audit->pos=0;
    }
  if((fseek(audit->file,audit->pos,SEEK_SET))!=0){
    nbLogMsg(context,0,'E',"File \"%s\" fseek failed errno=%d.",audit->fileName,errno);
    fclose(audit->file);
    audit->file=NULL;
    auditDisable(context,skillHandle,audit);
    return;
    }
  if(audit->trace) nbLogBar(context);
  while(fgets(logbuf,2048,audit->file)!=NULL){
    cursor=strchr(logbuf,'\n');
    if(cursor!=NULL) *cursor=0;
    if(audit->trace) nbLogPut(context,"] %s\n",logbuf);
    //nbLogPut(context,"] %s\n",logbuf);
    nbTranslatorExecute(context,audit->translatorCell,logbuf);
    if(audit->file==NULL){
      nbLogMsg(context,0,'W',"Node disabled during file processing");
      return;
      }
    }
  if(audit->trace) nbLogBar(context);
  audit->pos=ftell(audit->file);
  if(audit->trace) nbLogMsg(context,0,'T',"File size=%u",audit->pos);
  fclose(audit->file);
  audit->file=NULL;
  }

//==================================================================================
// Skill Methods
//==================================================================================

/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define <term> node audit("<filename>","<translator>",<schedule>);
*/
void *auditConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbAudit *audit;
  nbCELL fileNameCell,translatorNameCell,scheduleCell,nullCell;
  nbSET argSet;
  int type;
  char *fileName,*translatorName;
  nbCELL translatorCell;

  //nbLogMsg(context,0,'T',"auditConstruct: called");
  argSet=nbListOpen(context,arglist);
  fileNameCell=nbListGetCellValue(context,&argSet);
  if(fileNameCell==NULL){
    nbLogMsg(context,0,'E',"Expecting string file name as first parameter");
    return(NULL);
    }
  type=nbCellGetType(context,fileNameCell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string file name as first parameter");
    return(NULL);
    }
  fileName=nbCellGetString(context,fileNameCell); 

  translatorNameCell=nbListGetCellValue(context,&argSet);
  if(translatorNameCell==NULL){
    nbLogMsg(context,0,'E',"Expecting string translator name as second parameter");
    return(NULL);
    }
  type=nbCellGetType(context,translatorNameCell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string translator name as second parameter");
    return(NULL);
    }
  translatorName=nbCellGetString(context,translatorNameCell);  

  scheduleCell=nbListGetCell(context,&argSet);  // get schedule cell - not value
 
  nullCell=nbListGetCellValue(context,&argSet);
  if(nullCell!=NULL){
    nbLogMsg(context,0,'E',"The audit skill only accepts three parameters.");
    return(NULL);
    }

  translatorCell=nbTranslatorCompile(context,0,translatorName);
  if(translatorCell==NULL){
    nbLogMsg(context,0,'E',"Unable to load translator '%s'",translatorName);
    return(NULL);
    }
  audit=nbAlloc(sizeof(struct NB_MOD_AUDIT));
  audit->file=NULL;
  audit->pos=0;
  audit->fileNameCell=fileNameCell;
  audit->fileName=fileName;
  audit->translatorNameCell=translatorNameCell;
  audit->translatorName=translatorName;
  audit->translatorCell=nbCellGrab(context,translatorCell);
  audit->scheduleCell=scheduleCell;
  audit->synapseCell=NULL;
  audit->trace=0;
  
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  if(audit->trace) nbLogMsg(context,0,'T',"auditConstruct: returning");
  return(audit);
  }

/*
*  enable() method
*
*    enable <node>
*
*/
int auditEnable(nbCELL context,void *skillHandle,nbAudit *audit){
  if(audit->trace) nbLogMsg(context,0,'T',"auditEnable() called %s using",audit->fileName,audit->translatorName);
  if((audit->file=fopen(audit->fileName,"r"))==NULL){
    nbLogMsg(context,0,'E',"Unable to open audit file \"%s\".",audit->fileName);
    return(1);
    }
  if((fseek(audit->file,0,SEEK_END))!=0){
    nbLogMsg(context,0,'L',"Failed fseek on \"%s\" - errno=%d.",audit->fileName,errno);
    fclose(audit->file);
    audit->file=NULL;
    return(1);
    }
  audit->pos=ftell(audit->file);
  if(fclose(audit->file)!=0) nbLogMsg(context,0,'L',"File close failed - errno=%d %s",errno,strerror(errno));
  audit->file=NULL;
  audit->synapseCell=nbSynapseOpen(context,skillHandle,audit,audit->scheduleCell,auditAlarm);
  nbLogMsg(context,0,'I',"Enabled audit of %s using %s",audit->fileName,audit->translatorName);
  nbLogFlush(context);
  return(0);
  }


/*
*  disable method
* 
*    disable <node>
*/
int auditDisable(nbCELL context,void *skillHandle,nbAudit *audit){
  if(audit->trace) nbLogMsg(context,0,'T',"auditDisable() called");
  if(audit->synapseCell) audit->synapseCell=nbSynapseClose(context,audit->synapseCell);  // release the synapse
  else return(0);  // already disabled
  if(audit->file){
    if(fclose(audit->file)!=0) nbLogMsg(context,0,'L',"File close failed - errno=%d %s",errno,strerror(errno));
    audit->file=NULL;
    }
  audit->pos=0;
  nbLogMsg(context,0,'I',"Disabled audit of %s using %s",audit->fileName,audit->translatorName);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    table:trace,notrace
*/
int *auditCommand(nbCELL context,void *skillHandle,nbAudit *audit,nbCELL arglist,char *text){
  if(strstr(text,"notrace")) audit->trace=0;
  else if(strstr(text,"trace")) audit->trace=1;
  else nbLogMsg(context,0,'E',"Command not recognized.");
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
int auditDestroy(nbCELL context,void *skillHandle,nbAudit *audit){
  if(audit->trace) nbLogMsg(context,0,'T',"auditDestroy called");
  auditDisable(context,skillHandle,audit);
  nbCellDrop(context,audit->fileNameCell);
  nbCellDrop(context,audit->scheduleCell);
  nbCellDrop(context,audit->translatorCell);
  nbCellDrop(context,audit->translatorNameCell);
  nbFree(audit,sizeof(struct NB_MOD_AUDIT));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *auditBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,auditConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,auditDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,auditEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,auditCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,auditDestroy);
  return(NULL);
  }



