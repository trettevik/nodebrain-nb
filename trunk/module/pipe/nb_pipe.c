/*
* Copyright (C) 1998-2009 The Boeing Company
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
*
* Name:   nb_pipe.c
*
* Title:  NodeBrain Pipe Skill Module 
*
* Function:
*
*   This program is a NodeBrain skill module that accepts commands from
*   a pipe (fifo file).
*   
* Synopsis:
*
*   define server node pipe.server("<identity>@<filename>");
*   define client node pipe.client("<filename>");
*
* Description:
*
*   This skill module listens for input at a FIFO and passes each
*   line of input to the interpreter using the specified identity.
*   
* Defect:
*
*   We are not currently verifying that the file is a pipe.  If the file
*   is created as a normal file before we open it, we will endlessly
*   process a transaction---a very bad thing.  Need to check the file type
*   if it already exists.
*
* History:
*
*   The pipe module is a replacement for the now obselete FIFO
*   listener.
*
*   define l1 listener protocol="FIFO",file="<file>",identity="<identity>";
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2007/06/23 Ed Trettevik - original skill module prototype version
* 2007/06/23 eat 0.6.8  Structured skill module around old FIFO listener code
* 2007/07/22 eat 0.6.8  Modified reader to handle incomplete commands at end of buffer
*=====================================================================
*/
#include "config.h"
#include <nb.h>

//=============================================================================
 
typedef struct NB_MOD_PIPE_READER{
  struct IDENTITY *identity;      // identity
  char             idName[64];    // identity name
  char             filename[512]; // file name
  int              fildes;        // file descriptor
  char             buffer[NB_BUFSIZE];  // input buffer
  char            *cursor;        // location of next read
  unsigned char    ignore2eol;    // ignore to end of line
  unsigned char    trace;         // trace option
  } NB_MOD_PipeReader;

//==================================================================================
//
// Handle connection requests
//
//   this routine replaces fifoAlertListener
//
void pipeRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_PipeReader *pipe=handle;
  size_t len;
  char *bufcur,*bufeol;

  if(pipe->trace) nbLogMsg(context,0,'T',"pipeRead: called");
  len=read(pipe->fildes,pipe->cursor,sizeof(pipe->buffer)-(pipe->cursor-pipe->buffer));
  while(len==-1 && errno==EINTR){
    len=read(pipe->fildes,pipe->cursor,sizeof(pipe->buffer)-(pipe->cursor-pipe->buffer));
    }
  if(len>0){
    len+=(pipe->cursor-pipe->buffer);     // adjust length for part we already had maybe
    pipe->cursor=pipe->buffer;            // assume next read will be fresh until we learn otherwise
    if(pipe->ignore2eol){
      nbLogMsg(context,0,'W',"Ignoring to end of line");
      if((bufeol=memchr(pipe->buffer,'\n',len))==NULL){
        *(pipe->buffer+len)=0;
        nbLogPut(context,"] %s\n",pipe->buffer);
        return;  // continue reading until end of line or end of file
        }
      *bufeol=0;                                                  // drop LF - 10
      if(bufeol>pipe->buffer && *(bufeol-1)==13) *(bufeol-1)=0;   // drop CR - 13
      nbLogPut(context,"] %s\n",pipe->buffer);
      bufcur=bufeol+1;
      len-=bufeol+1-pipe->buffer;
      pipe->ignore2eol=0;
      }
    else{
      nbLogMsg(context,0,'I',"FIFO %s@%s",pipe->idName,pipe->filename);
      bufcur=pipe->buffer;
      }
    while(len>0 && (bufeol=memchr(bufcur,'\n',len))!=NULL){
      *bufeol=0;                                           /* drop LF - 10 */
      if(bufeol>bufcur && *(bufeol-1)==13) *(bufeol-1)=0;  /* drop CR - 13 */
      if(pipe->trace) nbLogPut(context,"] %s\n",bufcur);
      nbCmdSid(context,bufcur,1,pipe->identity);
      nbLogFlush(context);
      len-=bufeol+1-bufcur;
      bufcur=bufeol+1;
      }
    if(len>0){
      if(len>=sizeof(pipe->buffer)){
        *(pipe->buffer+sizeof(pipe->buffer)-1)=0;
        nbLogMsg(context,0,'E',"Command fills %d character buffer before end of line - ignoring to end of line.",sizeof(pipe->buffer));
        nbLogPut(context,"] %s\n",pipe->buffer);
        pipe->ignore2eol=1;
        pipe->cursor=pipe->buffer;
        }
      else{
        strncpy(pipe->buffer,bufcur,len);
        pipe->cursor=pipe->buffer+len;  // next read is continuation of last command;
        *(pipe->cursor)=0;
        if(pipe->trace) nbLogPut(context,"Looking for more to go with: %s\n",pipe->buffer);
        }
      }
    }
  else{
    if(pipe->trace) nbLogMsg(context,0,'T',"pipeRead: end of file reached");
    close(pipe->fildes);
    if(pipe->cursor!=pipe->buffer){
      *(pipe->cursor)=0;
      nbLogPut(context,"] %s\n",pipe->buffer);
      nbLogMsg(context,0,'E',"Command ended without newline character ignored.");
      }
    pipe->cursor=pipe->buffer;
    pipe->ignore2eol=0;
#if defined(WIN32)
    if((pipe->fildes=open(pipe->filename,O_RDONLY))<0){
#else
    if((pipe->fildes=open(pipe->filename,O_RDONLY|O_NONBLOCK))<0){
#endif
      nbLogMsg(context,0,'E',"pipeRead: unable to open FIFO %s (%d)",pipe->filename,pipe->fildes);
      }
    }
  }

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*
*    define <term> node pipe.server("<identity>@<filename>");
*/
void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_PipeReader *pipe;
  nbCELL cell=NULL;
  nbSET argSet;
  char *inCursor,*cursor;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string pipe specification as first parameter - identity@filename");
    return(NULL);
    }
  cursor=nbCellGetString(context,cell);
  pipe=malloc(sizeof(NB_MOD_PipeReader));
  inCursor=pipe->idName;
  while(*cursor==' ') cursor++;
  while(*cursor && *cursor!='@'){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*cursor!='@'){
    nbLogMsg(context,0,'E',"Identity not found in pipe specification - expecting identity@filename");
    return(NULL);
    }
  cursor++;
  pipe->identity=nbIdentityGet(context,pipe->idName);
  if(pipe->identity==NULL){
    nbLogMsg(context,0,'E',"Identity '%s' not defined",pipe->idName);
    free(pipe);
    return(NULL);
    }
  inCursor=pipe->filename;
  while(*cursor){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*pipe->filename==0){
    nbLogMsg(context,0,'E',"File name not found in pipe specification - expecting identity@filename");
    free(pipe);
    return(NULL);
    }
  pipe->fildes=0;
  *pipe->buffer=0;
  pipe->cursor=pipe->buffer;
  pipe->ignore2eol=0;
  pipe->trace=0;
  nbCellDrop(context,cell);
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(pipe);
  }

/*
*  enable() method
*
*    enable <node>
*/
int serverEnable(nbCELL context,void *skillHandle,NB_MOD_PipeReader *pipe){
#if defined(WIN32)
  if((pipe->fildes=open(pipe->filename,O_RDONLY))<0){
    // insert windows CreateNamedPipe here, unless we have to make separate funtions
#else
  if((pipe->fildes=open(pipe->filename,O_RDONLY|O_NONBLOCK))<0){
    if(mknod(pipe->filename,S_IFIFO|0600,0)<0){
      nbLogMsg(context,0,'E',"Unable to create FIFO %s - %s",pipe->filename,strerror(errno));
      return(1);
      }
#endif
#if defined(WIN32)
    if((pipe->fildes=open(pipe->filename,O_RDONLY))<0){
#else
    if((pipe->fildes=open(pipe->filename,O_RDONLY|O_NONBLOCK))<0){
#endif
      nbLogMsg(context,0,'E',"Unable to open FIFO %s - %s",pipe->filename,strerror(errno));
      return(1);
      }
    }
  nbListenerAdd(context,pipe->fildes,pipe,pipeRead);
  nbLogMsg(context,0,'I',"Listening for FIFO connections as %s@%s",pipe->idName,pipe->filename);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int serverDisable(nbCELL context,void *skillHandle,NB_MOD_PipeReader *pipe){
  nbListenerRemove(context,pipe->fildes);
  close(pipe->fildes);
  pipe->fildes=0;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    <node>:trace,notrace
*/
int *serverCommand(nbCELL context,void *skillHandle,NB_MOD_PipeReader *pipe,nbCELL arglist,char *text){
  if(strstr(text,"notrace")) pipe->trace=0;
  else if(strstr(text,"trace")) pipe->trace=1;
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
int serverDestroy(nbCELL context,void *skillHandle,NB_MOD_PipeReader *pipe){
  if(pipe->trace) nbLogMsg(context,0,'T',"serverDestroy called");
  if(pipe->fildes!=0) serverDisable(context,skillHandle,pipe);
  free(pipe);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *serverBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,serverConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,serverDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,serverDestroy);
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *readerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,serverConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,serverDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,serverDestroy);
  return(NULL);
  }

//=============================================================================================

typedef struct NB_MOD_PIPE{
  nbCELL  filenamecell;      // cell containing file name - drop on destroy
  char   *filename;          // file name
  int     fildes;            // file descriptor
  char    buffer[NB_BUFSIZE];      // input buffer
  } NB_MOD_Pipe;

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*
*    define <term> node pipe.reader("<identity>@<filename>");
*/
void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Pipe *pipe;
  nbCELL cell;
  nbSET argSet;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string pipe file name as first parameter");
    return(NULL);
    }
  pipe=malloc(sizeof(NB_MOD_Pipe));
  pipe->filenamecell=cell;
  pipe->filename=nbCellGetString(context,cell);
  return(pipe);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *clientCommand(nbCELL context,void *skillHandle,NB_MOD_Pipe *pipe,nbCELL arglist,char *text){
  sprintf(pipe->buffer,"%s\n",text);
  pipe->fildes=open(pipe->filename,O_WRONLY|O_APPEND);
  write(pipe->fildes,pipe->buffer,strlen(pipe->buffer));
  close(pipe->fildes);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
int clientDestroy(nbCELL context,void *skillHandle,NB_MOD_Pipe *pipe){
  nbCellDrop(context,pipe->filenamecell);
  free(pipe);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *clientBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *pipeBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }

