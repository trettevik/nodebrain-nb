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
* File:     nbterm.c 
*
* Title:    Term Glossary Management Routines (prototype)
*
* Function:
*
*   This file provides routines that manage Nodebrain terms and the
*   related concepts of glossaries and contexts.  These routines are called by
*   the command interpreter to locate "named" objects.
*
* Synopsis:
*
*   #include "nb.h"
*
*     NB_Term *nbTermNew(context,name,value);
*
* Description
*
*   You can then construct some terms using the nbTermNew() function.
*
*     NB_Term *myterm=nbTermNew(context,name,value);
*
*   The nbTermNew() function will obtain a reservation on the "value" object.
*   However, if you assign a pointer to the term in another structure, you
*   must ensure that the term is reserved.  This may be accomplished automatically
*   when passing the pointer to another constructor.  Otherwise, use the 
*   grabObject() function.
*
*     mystructPtr=grabObject(myterm);
*  
*   When you are finished with the term, use dropObject to decrement the
*   reference count.
*
*     termDrop(NB_Term *myterm);
*
*   To locate an existing term, use symLoc.  A null pointer is returned
*   if the symbol is not found.
*
*     NB_Term *myterm=termLoc(context,name);
*
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/06/28 Ed Trettevik (original prototype version)
*             1) This code is a revision to code previously imbedded
*                in nodebrain.c. The primary difference is the use of nbobj.h
*                routines.
* 2001/07/02 eat - version 0.2.8
*             1) Eliminated TERM/STR structure in favor of TERM/STRING.
* 2001/07/04 eat - version 0.2.8
*             1) Changed from VARIABLE to NB_Cell.
* 2001/12/27 eat - version 0.2.9c
*             1) Bug patch - nbTermFindDown wasn't recognizing termP==NULL
* 2002/02/12 eat - version 0.2.9A
*             1) Included typeTranslator
*
* 2003/02/24 eat 0.5.0  Replaced gets() with fgets() in termAskUser().
*
* 2003/03/03 eat 0.5.1  Conditional compile for Max OS X [ see defined(MACOS) ]
*
* 2003/10/07 eat 0.5.5  Replaced fgets() with nbGets().
* 2003/11/18 eat 0.5.5  Working on eliminating the term.class attribute.
* 2004/09/25 eat 0.6.1  Limit term assignment to term owner
* 2004/10/04 eat 0.6.1  Conditionals for FreeBSD, OpenBSD, and NetBSD
* 2006/01/13 eat 0.6.4  Modified termUndef() to allow unlinking of subordinate terms
*            This is handled in an ideal way still.  We should have a return
*            code from termUndef() to let us know if a term was undefined.  This
*            could be used when undefining subordinates to enable the unlinking
*            to occur as we spin through the terms.  This would be more efficient.
* 2008/01/20 eat 0.6.9  Disabled reference feature to improve performance
* 2008/01/21 eat 0.6.9  Converted to binary tree glossaries - replacing hash and list
* 2008/02/09 eat 0.6.9  Introduced nb_Undefined object
*            The nb_Undefined object is now used as an alternative to nb_Unknown in
*            the def field of terms to distinguish from the being assigned the
*            Unknown value.  Was incorrectly allowing redefinition of terms defined
*            to have the Unknown value.
* 2008-11-11 eat 0.7.3  Changed failure exit code to NB_EXITCODE_FAIL
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2011-02-08 eat 0.8.5  Stopped echo of nbTermOptionString values
* 2012-01-16 dtl 0.8.7  Checker updates
* 2012-10-17 eat 0.8.12 Changed name from termGetName to nbTermName
* 2012-10-17 eat 0.8.12 Added size parameter
* 2012-12-15 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>
#include <stddef.h>

NB_Term *termFree=NULL;
NB_Term *gloss;      /* root context term */
struct TYPE *termType;
NB_Term *addrContext=NULL;   /* current context term (local)  */
NB_Term *symContext=NULL;    /* symbolic context */

/**********************************************************************
*  String Maintenance Routines
*
*/
/*
*  Ask user for string value
*    Value must be NB_BUFSIZE characters;
*/    
static void termAskUser(char *name,char *value){
  outPut("\nEnter cell %s==",name);
  outFlush();
  nbGets(0,value,NB_BUFSIZE);
  }

/* 
*  Ask file for string value
*
*     File line format is
*        name:value
*
*  Return Code:
*    -2 - file contains value string too long for buffer
*    -1 - Unable to open file
*     0 - value not found
*     1 - value found 
*/
static int termAskFile(char *name,char *value,size_t size,char *filename){
  char buffer[NB_BUFSIZE];
  int len=strlen(name);
  FILE *file;
  char *cursor;
  
  *value=0;
  outMsg(0,'T',"Resolving \"%s\" via file : %s",name,filename);
  if((file=fopen(filename,"r"))==NULL){
    outMsg(0,'E',"termAskFile: Unable to open '%s'",filename);
    return(-1);
    }     
  while(fgets(buffer,NB_BUFSIZE,file)!=NULL){
    if(strncmp(buffer,name,len)==0 && *(buffer+len)==':'){
      cursor=buffer+len+1;
      len=strlen(cursor);
      if(len==0){
        fclose(file);    // 2012-12-18 eat - CID 751608
        return(0);
        }
      if(*(cursor+len-1)=='\n'){
        *(cursor+len-1)=0;
        len--;
        }
      if(len>=size){
        outMsg(0,'E',"termAskFile: Value too long for buffer - file '%s' term '%s' value:%s",filename,name,cursor);
        fclose(file);
        return(-2);
        }
      strcpy(value,cursor);
      fclose(file);
      return(1);
      }
    }
  fclose(file);
  return(0);
  } 
   
/*
*  Ask command for string value
*    Note: This will not work on windows because it uses popen
*/   
static void termAskCommand(char *name,char *value,size_t size,char *command){
#if defined(WIN32)  
  outMsg(0,'E',"termAskCommand: Not supported on windows");
  strcpy(value,"?"); 
#else
  FILE *file;
  char buffer[NB_BUFSIZE];
  char cmd[NB_BUFSIZE];
  int rc,len;
  //char *cursor;

  if(strlen(command)+strlen(name)+3>=sizeof(cmd)){
    outMsg(0,'E',"Command and name too long for buffer.");
    return;
    }
  strcpy(cmd,command);
  strcat(cmd," \"");   // 2013-01-01 eat - VID 5454-0.8.13-1 FP
  strcat(cmd,name);    // 2013-01-01 eat - VID 5454-0.8.13-1 FP
  strcat(cmd,"\"");    // 2013-01-01 eat - VID 5425-0.8.13-1 FP
  outMsg(0,'T',"Resolving \"%s\" via command : %s",name,cmd);
  if(size<2){
    outMsg(0,'L',"termAskCommand: return buffer too small - size=%d",size);
    return;
    }
  strcpy(value,"?");
//#if !defined(mpe) && !defined(ANYBSD)
//  signal(SIGCLD,SIG_DFL);
//#endif
  if((file=popen(cmd,"r"))==NULL){
    outMsg(0,'E',"Unable to execute command. errno=%d",errno);
    return;
    }
  if(fgets(buffer,sizeof(buffer),file)!=NULL){
    len=strlen(buffer);
    if(len>0 && *(buffer+len-1)=='\n'){
      *(buffer+len-1)=0;
      len--;
      }
    if(len>=size){
      outMsg(0,'L',"termAskCommand: return string too long for buffer - len=%d buffer size=%d",len,size);
      pclose(file);
      return;
      }
    strcpy(value,buffer);
    outPut("Value=(%s)\n",value);
    }
  else outMsg(0,'E',"No value returned.");   
  rc=pclose(file);   
  if(rc) outMsg(0,'I',"Exit Code (%d)",rc);
//#if !defined(mpe) && !defined(ANYBSD)
//  signal(SIGCLD,SIG_IGN);
//#endif
#endif
  }

/*
*  Display the conditions registered for impact by fact change
*/
void termPrintConditions(NB_Term *term){
  NB_TreeIterator treeIterator;
  NB_TreeNode *treeNode;
  NB_Cell *cell;

  NB_TREE_ITERATE(treeIterator,treeNode,((NB_Cell *)term)->sub){
    cell=(NB_Cell *)treeNode->key;
    outPut("\n  ");
    printObject((NB_Object *)cell);
    NB_TREE_ITERATE_NEXT(treeIterator,treeNode)
    }
  outPut("\n");
  outFlush();
  }

/*
*  Get a definition for a term whose definition is Unknown
*/
void termResolve(NB_Term *term){
  NB_Term *context;
  NB_Object *object;
  char value[NB_BUFSIZE],*cursor;
  char name[1024];

  *value=0;   /* initialize value to empty string */ 
  context=term->context;
  /* locate and execute "source" command */
  while(context!=NULL && (context->def->type!=nb_NodeType ||  
   ((NB_Node *)context->def)->source==(struct STRING *)nb_Unknown))
    context=context->context;

  if(context==NULL){  /* ask user if we don't find a source command */
    nbTermName(name,sizeof(name),term,addrContext);
    if(!nb_opt_prompt){
      outMsg(0,'W',"No consultant for %s",name);
      return;
      }
    //termPrintConditions(((NB_Cell *)term)->sub);
    termPrintConditions(term);
    termAskUser(name,value);
    }
  else{
    struct STRING *string=((NB_Node *)context->def)->source; 
    nbTermName(name,sizeof(name),term,context);
    if(*(string->value)=='<') termAskFile(name,value,sizeof(value),string->value+1);
    else termAskCommand(name,value,sizeof(value),string->value);
    }
  cursor=value;
  for(context=term->context;context!=NULL && context->def->type!=nb_NodeType;context=context->context);
  object=nbParseCell(context,&cursor,0);
  if(object==NULL) object=nb_Unknown;
  term->def=grabObject(object);
  if(object->value!=object){
    nbCellEnable((NB_Cell *)object,(NB_Cell *)term);
    term->cell.object.value=grabObject(nbCellSolve_((NB_Cell *)object));
    }
  else term->cell.object.value=grabObject(term->def);
  nbCellPublish((NB_Cell *)term);
  nbCellReact();
  }
  
/*==========================================================================
*  Term Management 
*/
void disableY();
void schedPrintDump();

/*=====================================================================
*  Term Glossary Management Routines 
*====================================================================*/
/*
*  Object Management Methods
*/
/*
void termPrint3(NB_Term *term){
  printStringRaw(term->word);
  }
*/

/*
*  Solve for a term with an unknown value
*/
void solveTerm(NB_Term *term){
  if(trace){
    outMsg(0,'T',"solveTerm(): called");
    outPut("Term: ");
    printObject((NB_Object *)term);
    outPut("\n");
    }
  if(term->def==nb_Unknown) termResolve(term);
  else nbCellSolve_((NB_Cell *)term->def);
  return;
  }

/*
*  Evaluate and return the value of a term - value of assigned object
*/
NB_Object *evalTerm(NB_Term *term){
  return(((NB_Object *)term->def)->value);
  }

/*
*  Enable a term function by enabling the assigned function.
*    Don't worry, nbCellEnable accepts NULL and non-function objects.
*    Calls to "enable" methods are followed by calls to "eval" methods
*    so we don't need to set a value here. 
*/
void enableTerm(NB_Term *term){
  nbCellEnable((NB_Cell *)term->def,(NB_Cell *)term);
  }

/*
*  Disable a term function by disabling the assigned function.
*    Don't worry, nbCellDisable accepts NULL and non-function objects.
*/
void disableTerm(NB_Term *term){
  if(term->cell.object.value==nb_Disabled) return;
  if(term->cell.object.value==term->def) return; /* static value */
  nbCellDisable((NB_Cell *)term->def,(NB_Cell *)term);
  dropObject(term->cell.object.value); /* 2004/08/28 eat */
  term->cell.object.value=nb_Disabled;
  }

void destroyTerm(NB_Term *term){
  //NB_Term *lterm,**termP;
  NB_Term *context;
  NB_TreePath treePath;
  NB_TreeNode *treeNode;

  if(trace) outMsg(0,'T',"destroyTerm() called for %s",term->word->value);
  if(term->def!=NULL) term->def=dropObject(term->def);
  if(term->cell.object.value!=NULL) term->cell.object.value=dropObject(term->cell.object.value);  // 2006-01-13
  if(term->terms!=NULL){
    //if(term->def!=NULL) dropObject(term->def);                // 2006-01-13
    term->cell.object.value=nb_Disabled;
    term->def=nb_Undefined;
    return;
    }
  if((context=term->context)!=NULL){ 
    treeNode=nbTreeLocate(&treePath,term->word,(NB_TreeNode **)&context->terms);
    if(treeNode==NULL) outMsg(0,'L',"destroyTerm() Term \"%s\" not found in context.",term->word->value);  
    else nbTreeRemove(&treePath);
    }
  term->word=dropObject(term->word);
  term->cell.object.next=(NB_Object *)termFree;
  termFree=term;
  if(trace) outMsg(0,'T',"destroyTerm() returning");
  }


/**********************************************************************
*  Public Methods
*
*  These routines manage term glossaries.  A glossary is a hierarchical
*  structure.  Each term in a glossary has a sub glossary.  An
*  identifier of the form "job.start.date" has three levels of 
*  qualification.  Each level of qualification is a term which
*  represents both an object and a glossary.  We say the term "date"
*  is defined within the glossary "job.start"
*
*  A "term hierarchy" is a tree structure of terms defined by the
*  term.parent [term.context] (parent glossary) and 
*  term.glossary [term.terms] (children) fields.
*
*  A "context hierarchy" is a "term hierarchy" where all the
*  terms involved are of class Context.  A Context term may only have
*  another context as a parent.
*
*  A "class hierarchy" is defined by the term.def (super) field for
*  terms whose class is "class".  There is no link from a class term to
*  all terms of that class. 
*
*  There are three types of term glossaries: instance, object, and
*  context.
*
*  An "instance glossary" contains "term specific" subordinate term
*  definitions.
*
*  An "object glossary" contains definitions in an instance glossary,
*  plus definitions inherited via a term's class hierarchy.  Terms are
*  inherited from the object glossary of a term's class and super
*  classes.
*
*  A "context glossary" contains the definitions in a context term's
*  object glossary and definitions inherited via the context
*  hierarchy. 
*
*
*  A class is a term whose class is "class".  Classes are
*  extentions of other classes, forming a class hierarchy.  A term's
*  "class glossary" is the set of all terms inherited via its class
*  hierarchy.  A term's "object glossary" is the combination of its
*  instance glossary and class glossary.
*
*  There are three routines for searching instance, object and context
*  glossaries.
*
*     termLocI  - instance glossary
*     nbTermFindDown - object glossary 
*     nbTermFind     - context glossary
*
*        
*/
void printTerm(NB_Term *term){
  termPrintName(term);
  }

void initTerm(NB_Stem *stem){
  termType=newType(stem,"term",NULL,0,printTerm,destroyTerm);
  termType->apicelltype=NB_TYPE_TERM;
  nbCellType(termType,solveTerm,evalTerm,enableTerm,disableTerm);
  }

/*
*  nbTermFindHere():
*
*  Find term within a specified context.
*
*  Returns term handle (NULL if not found). 
*
*/

// NOTE: Without the trace stuff, this is only three lines.  It
//       will be better to replace the calls to this with the
//       three lines to reduce calls.

NB_Term *nbTermFindHere(NB_Term *term,NB_String *word){
  NB_TreeNode *treeNode=term->terms;

  NB_TREE_FIND((void *)word,treeNode)
  if(treeNode==NULL) return(NULL);
  term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
  return(term);
  }

NB_Term *nbTermFindDot(NB_Term *term,char **qualifier){
  char *cursor=*qualifier+1;
  if(**qualifier!='.') return(term);
  while(*cursor=='.'){   /* leading periods mean go up */
    if(NULL==(term=term->context)) return(NULL);
    cursor++;
    }
  *qualifier=cursor;
  return(term);
  }  

/*
*  nbTermFindInScope:
*
*  Find term up the context hiearchy.
*
*  Returns term pointer, NULL if not found.
*
*/
NB_Term *nbTermFindInScope(NB_Term *term,char *qualifier){
  NB_Term *termFound=NULL;
  NB_String *word;
  char *cursor=qualifier;

  if(*qualifier=='$') cursor++; /* Ignore symbolic prefix in lookup */
  else if(*qualifier=='%'){
    term=symContext; /* Switch context for symbolic variable */
    cursor++;
    }
  else if(strcmp("_",qualifier)==0) return(gloss);    /* root context reference */
  else if(strcmp("@",qualifier)==0) return(locGloss); /* local root context reference */
  else if(*qualifier=='@') term=gloss;     /* Switch context for brain context reference */
  if(*cursor=='.' && (NULL==(term=nbTermFindDot(term,&cursor)))) return(NULL);
  word=grabObject(useString(cursor));
  while(termFound==NULL && term!=NULL){
    // it may be worth repeating nbTermFindHere logic here to reduce calls
    termFound=nbTermFindHere(term,word);
    term=term->context;
    }
  dropObject(word);
  return(termFound);
  }

/*
*  nbTermFind:
*
*  Find the term associated with an identifier in a given context.
*/
NB_Term *nbTermFind(NB_Term *term,char *identifier){ 
  NB_String *word;
  char *cursor=identifier;
  char qualifier[256];
  char *qursor=qualifier;

  if((cursor=getQualifier(qualifier,cursor))==NULL) return(NULL);
  if(*qualifier!='.'){
    if(strcmp("@",qualifier)==0) term=locGloss;
    else if(NULL==(term=nbTermFindInScope(term,qualifier))) return(NULL);
    }
  else{
    if(NULL==(term=nbTermFindDot(term,&qursor))) return(NULL);
    if(*qursor==0) return(term);
    word=grabObject(useString(qursor));
    term=nbTermFindHere(term,word);
    dropObject(word);
    if(term==NULL) return(NULL);
    } 
  while(*cursor!=0 && *cursor!='}'){
    if((cursor=getQualifier(qualifier,cursor))==NULL) return(NULL);
    word=grabObject(useString(qualifier));
    term=nbTermFindHere(term,word);
    dropObject(word);
    if(term==NULL) return(NULL);
    }
  return(term);
  }

NB_Term *nbTermFindDown(NB_Term *term,char *identifier){
  NB_String *word;
  char *cursor=identifier;
  char qualifier[256];

  while(*cursor!=0 && *cursor!='}'){
    if((cursor=getQualifier(qualifier,cursor))==NULL) return(NULL);
    word=grabObject(useString(qualifier));
    term=nbTermFindHere(term,word);
    dropObject(word);
    if(term==NULL) return(NULL);
    }
  return(term);
  }

/*
*  Build new term structure with NULL value and glossary 
*/
NB_Term *makeTerm(NB_Term *context,NB_String *word){
  NB_Term *term;

  if(trace) outMsg(0,'T',"makeTerm calling nbCellNew");
  term=nbCellNew(termType,(void **)&termFree,sizeof(NB_Term));
  term->context=context; 
  term->terms=NULL;                       /* change to term->gloss */
  term->def=nb_Undefined;  
  term->word=word;
  grabObject(term);
  if(trace) outMsg(0,'T',"makeTerm returning");
  return(term);  
  }
  
/*
*  Assign new term value and definition.
*
*  2004/09/25 eat - limit assignment to owner
*/
void nbTermAssign(NB_Term *term,NB_Object *new){
  NB_Object *old;

  if(trace) outMsg(0,'T',"nbTermAssign called.");
  /* make sure clientIdentity is the owner of the term's context */
/* Need to test this and get it working - had a problem with logfile listener
  if(((NB_Node *)term->context->def)->owner!=clientIdentity){
    outMsg(0,'E',"%s does not own %s and may not change value or definition",clientIdentity->name->value,term->word->value);
    return;
    }
*/
  if(new==NULL) new=nb_Unknown; /* we could make sure this doesn't happen */
  if(term->def==new) return;
  if(term->def!=nb_Unknown && term->def!=nb_Disabled){
    nbCellDisable((NB_Cell *)term->def,(NB_Cell *)term);  
    dropObject(term->def); 
    }
  term->def=grabObject(new);
  if((old=term->cell.object.value)==nb_Disabled){
    /* promote static values to term object value immediately */
    if(new->value==new) term->cell.object.value=grabObject(new);
    return; /* don't need to fuss more */
    }
  /* Otherwise, update value */
  dropObject(term->cell.object.value);
  if(new->value!=new){         /* cell */
    if(((NB_Cell *)new)->level>=term->cell.level){
      term->cell.level=((NB_Cell *)new)->level+1;
      nbCellLevel((NB_Cell *)term);       /* adjust level */
      }
    if(term->cell.sub==NULL){  /* disable if no subscribers */
      term->cell.object.value=nb_Disabled;
      return;
      }
    nbCellEnable((NB_Cell *)new,(NB_Cell *)term);
    }
  else term->cell.level=0;
  term->cell.object.value=grabObject(new->value);
  if(term->cell.object.value!=old) nbCellPublish((NB_Cell *)term);
  if(trace) outMsg(0,'T',"nbTermAssign returning.");
  }

/*
*  Define a new term.
*/
NB_Term *nbTermNew(NB_Term *context,char *ident,void *def){
  NB_Term *term=NULL;
  //NB_Term **termP;
  NB_String *word=NULL;
  NB_TreePath treePath;
  NB_TreeNode *treeNode;
  char qualifier[256],*cursor;
  //int match;

  if(trace) outMsg(0,'T',"nbTermNew() called.");
  if(ident==NULL){
    outMsg(0,'L',"nbTermNew() called with NULL identifier.");
    return(NULL);
    }
  if(def==NULL){
    outMsg(0,'L',"nbTermNew() called for \"%s\" with NULL definition - deprecated support will be removed",ident);
    def=nb_Undefined;
    }
  if(context==NULL){
    if(trace) outMsg(0,'T',"nbTermNew() called for \"%s\" with NULL context.",ident);
    if(strlen(ident)>=sizeof(qualifier)){
      outMsg(0,'E',"Terms may not be longer than 255 characters.");
      return(NULL);
      }
    strcpy(qualifier,ident);
    word=grabObject(useString(qualifier));
    term=makeTerm(context,word);
    }
  else{ /* locate the qualified term */
    if(trace){
      outMsg(0,'T',"nbTermNew() called for \"%s\" with context.",ident);
      outPut("Context is ");
      printObject((NB_Object *)context);
      outPut("\n");
      }
    cursor=ident;

    // adjust to leading periods 
    if(*cursor=='.'){
      context=nbTermFindDot(context,&cursor);
      if(context==NULL){
        outMsg(0,'L',"makeTerm: undefined context");
        return(NULL);
        }
      }
    while(*cursor!=0){
      cursor=getQualifier(qualifier,cursor);
      // This should only be done for the first qualifier, but just checking to see if this is the problem
      // NOTE: nbTermNew gets called when "@" is first defined and we don't yet have locGloss
      if(strcmp("@",qualifier)==0 && *cursor!=0) term=locGloss;
      else{
        word=grabObject(useString(qualifier));
        if(NULL==(treeNode=nbTreeLocate(&treePath,word,&context->terms))){
          term=makeTerm(context,word);
          nbTreeInsert(&treePath,(NB_TreeNode *)&term->left);
          }
        else{
          term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
          dropObject(word);
          }
        }
      context=term;
      }
    if(term->def==nb_Undefined){
      if(trace) outMsg(0,'I',"match on undefined term.");
      // 2006-10-28 eat - enable definition if reusing an enabled undefined term
      // 2008-02-17 eat - this is causing problems
      //if(((NB_Cell *)def)->level>=term->cell.level){
      //  term->cell.level=((NB_Cell *)def)->level+1;
      //  nbCellLevel(term);       /* adjust level */
      //  }
      // 2006-12-22 eat - when ready, investigate the following condition
      if(term->cell.object.value!=nb_Disabled) nbCellEnable(def,(NB_Cell *)term);
      }
    else{
      outMsg(0,'L',"Term \"%s\" may not be redefined.",ident);
      return(NULL);
      }
    }
  term->def=grabObject(def);
  /* in the future check term->def->type->attributes for TYPE_CELL bit */
  if(term->def->value==term->def)                   /* static object */
    term->cell.object.value=grabObject(def);        /* don't wait for enable */
  else term->cell.level=((NB_Cell *)def)->level+1;  /* cell if not static object */
  //outMsg(0,'T',"nbTermNew returning term - new or reused");
  return(term);
  }
  
void termUndefTree(NB_TreeNode *treeNode){
  NB_Term *term;
  if(treeNode==NULL) return;
  term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
  termUndefTree(term->left);   // undefine left branch
  termUndefTree(term->right);  // undefine right branch
  termUndef(term);             // undefine root node
  }

void termUndef(NB_Term *term){
  /*
  *  Undefine a term - remove it from a context.
  */
  //NB_Term *lterm,*nterm;
  if(((NB_Object *)term)->refcnt>1){
    outMsg(0,'E',"Term \"%s\" still referenced.",term->word->value);
    return;
    }
  if(trace){
    outMsg(0,'T',"termUndef called.");
    nbTermShowItem(term);
    }
  /* undefine all subordinate terms first */
  if(trace) outMsg(0,'T',"undefining subordinate terms");
  termUndefTree((NB_TreeNode *)term->terms);
  if(trace) outMsg(0,'T',"done with subordinate terms");
  dropObject(term); /* this should destroy anything we can destroy */
  if(trace) outMsg(0,'T',"termUndef returning");
  }


void termUndefAll(void){
  /*
  *  Undefine all terms.
  *
  *  Note: A much more efficient algorithm can be applied here because
  *        we don't need to worry about unlinking structures from other
  *        structures that will be deleted later.  This is just a prototype.
  */
  }  
  
// 2012-10-17 eat - changed name from termGetName to nbTermName
// 2012-10-17 eat - added size parameter
void nbTermName(char *name,size_t size,NB_Term *term,NB_Term *refContext){
  char *qual[50];  
  NB_Term *context;
  int n,level=0;
  char *cursor=name,*curlast=name+size-1;
  
  *name=0;
  if(term==gloss) return; //dtl: moved *name=0 to here for Checker // 2012-10-17 eat - changed back just to see what happens
  if(size<1024){
    outMsg(0,'L',"nbTermName: name buffer must be at least 1024 characters");
    exit(NB_EXITCODE_FAIL);
    }
  if(term==locGloss){  /* special case to avoid next special case */
    strcpy(name,"@");
    return;
    }
  if(term==refContext){ /* special case when term is the reference context */
    strcpy(name,".");
    return;
    }
  if(term->cell.object.type!=termType){
    outMsg(0,'L',"nbTermName: term address calculation error");
    exit(NB_EXITCODE_FAIL);
    }
  qual[0]=term->word->value;
  context=term->context;
  for(level=1;level<50 && context!=gloss && context!=refContext && context!=symContext && context!=NULL;level++){
    //outMsg(0,'T',"nbTermName reverse level=%d",level);
    qual[level]=context->word->value;
    //outMsg(0,'T',"nbTermName qualifier=%s",qual[level]);
    context=context->context;
    }
  for(level=level-1;level>=0;level--){ // 2012-12-28 eat - fixed defect dropping last char of each qualifier
    n=strlen(qual[level]);
    if(n+level>=curlast-cursor){       // truncate full name at qualifier if necessary
      strcpy(cursor,"?");
      return;
      }
    strncpy(cursor,qual[level],n);
    cursor+=n;
    if(level>0){
      *cursor='.';
      cursor++;
     }
    }
  *cursor=0;
  }
  
void termPrintName(NB_Term *term){
  char name[1024];
  
  nbTermName(name,sizeof(name),term,addrContext);
  outPut("%s",name);
  } 
 
void termPrintFullName(NB_Term *term){
  char name[1024];
  
  nbTermName(name,sizeof(name),term,gloss);
  outPut("%s",name);
  }   

void nbTermPrintLongName(NB_Term *term){
  char name[1024];
  nbTermName(name,sizeof(name),term,locGloss);
  outPut("%s",name);
  }

/*
*  Show a term as an item in a list
*/
void nbTermShowItem(NB_Term *term){
  if(term->cell.object.type!=termType){
    outMsg(0,'L',"termPrintGlossTree: term address calculation error");
    exit(NB_EXITCODE_FAIL);
    }
  printObject((NB_Object *)term);
  outPut(" ");
  outPut("= ");
  printObject(term->cell.object.value);
  if(term->def!=NULL && term->def!=term->cell.object.value){
    outPut(" == ");
    if(term->def==NULL) outPut("???");
    else printObjectItem(term->def);
    }
  outPut("\n");  
  }

void nbTermShowGlossTree(NB_TreeNode *treeNode){
  NB_Term *term;
  if(treeNode==NULL) return;
  term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
  nbTermShowGlossTree(term->left);
  nbTermShowItem(term);
  nbTermShowGlossTree(term->right);
  }

void nbTermShowGloss(NB_Term *term){
  if(trace) outMsg(0,'T',"nbTermShowGloss called");
  nbTermShowGlossTree((NB_TreeNode *)term->terms);
  }

void nbTermShowReport(NB_Term *term){
  nbTermShowItem(term);
  if(term->def!=NULL) printObjectReport(term->def);
  if(term->terms!=NULL) nbTermShowGloss(term);
  }

void termPrintGlossTree(NB_TreeNode *treeNode,NB_Type *type,int attr){
  NB_Term *term;
  if(treeNode==NULL) return;
  term=(NB_Term *)(((char *)treeNode)-offsetof(struct NB_TERM,left));
  if(term->cell.object.type!=termType){
    outMsg(0,'L',"termPrintGlossTree: term address calculation error");
    exit(NB_EXITCODE_FAIL);
    }
  termPrintGlossTree(term->left,type,attr);
  termPrintGloss(term,type,attr);
  termPrintGlossTree(term->right,type,attr);
  }

void termPrintGloss(NB_Term *term,NB_Type *type,int attr){
  /* we insist on term->def having a non-NULL and valid value */
  if((type==NULL || term->def->type==type) && (attr==0 || (term->def->type->attributes&attr)))
    nbTermShowItem(term);
  termPrintGlossTree((NB_TreeNode *)term->terms,type,attr);
  }

void termPrintGlossHome(NB_Term *term,NB_Type *type,int attr){
  NB_Term *addrContextSave=addrContext;
  addrContext=term;
  termPrintGloss(term,type,attr);
  addrContext=addrContextSave;
  }

/*******************************************************************************************
*  API Functions
*    We are limiting the data types required by API code, so NB_Term and NB_Object both
*    map to NB_Cell in the API functions.
*******************************************************************************************/

NB_Cell *nbTermCreate(NB_Cell *context,char *identifier,NB_Cell *definition){
  return((NB_Cell *)nbTermNew((NB_Term *)context,identifier,definition));
  }

NB_Cell *nbTermLocate(NB_Cell *context,char *identifier){
  return((NB_Cell *)nbTermFind((NB_Term *)context,identifier));
  }

NB_Cell *nbTermLocateHere(NB_Cell *context,char *identifier){
  return((NB_Cell *)nbTermFindDown((NB_Term *)context,identifier));
  }

NB_Cell *nbTermGetDefinition(NB_Cell *context,NB_Cell *term){
  return((NB_Cell *)(((NB_Term *)term)->def));
  }

NB_Cell *nbTermSetDefinition(NB_Cell *context,NB_Cell *term,NB_Cell *definition){
  nbTermAssign((NB_Term *)term,(NB_Object *)definition);
  return((NB_Cell *)(((NB_Term *)term)->cell.object.value));
  }

char *nbTermGetName(NB_Cell *context,NB_Cell *term){
  return(((NB_Term *)term)->word->value);
  }

void nbTermPrintGloss(nbCELL context,nbCELL term){
  termPrintGlossHome((NB_Term *)term,NULL,0);
  }

nbCELL nbSetContext(nbCELL context){
  nbCELL oldContext=(nbCELL)addrContext;
  addrContext=(NB_Term*)context;
  return(oldContext);
  }

char *nbTermOptionStringTrueFalseUnknown(nbCELL context,char *name,char *defaultTrue,char *defaultFalse,char *defaultUnknown){
  char *value=defaultUnknown;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL && (cell=nbCellCompute(context,cell))!=NULL){
    if(cell==(nbCELL)NB_OBJECT_FALSE) value=defaultFalse;
    else switch(nbCellGetType(context,cell)){
      case NB_TYPE_STRING: value=nbCellGetString(context,cell); break;
      case NB_TYPE_REAL: value=defaultTrue; break;
      } 
    }
  nbLogMsg(context,0,'T',"%s=%s",name,value);
  return(value);
  }

char *nbTermOptionStringSilent(nbCELL context,char *name,char *defaultValue){
  char *value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
//    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && (cell=nbCellCompute(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_STRING) value=nbCellGetString(context,cell);
  return(value);
  }

char *nbTermOptionString(nbCELL context,char *name,char *defaultValue){
  char *value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
//    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && (cell=nbCellCompute(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_STRING) value=nbCellGetString(context,cell);
  nbLogMsg(context,0,'T',"%s=%s",name,value);
  return(value);
  }

int nbTermOptionInteger(nbCELL context,char *name,int defaultValue){
  int value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_REAL) value=(int)nbCellGetReal(context,cell);
  nbLogMsg(context,0,'T',"%s=%d",name,value);
  return(value);
  }

double nbTermOptionReal(nbCELL context,char *name,double defaultValue){
  double value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_REAL) value=nbCellGetReal(context,cell);
  nbLogMsg(context,0,'T',"%s=%d",name,value);
  return(value);
  }
