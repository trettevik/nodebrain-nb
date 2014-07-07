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
* 2011/06/23 eat 0.8.6  Remove and add listener when pipe is closed and reopened
*            This is necessary because when a lower file descriptor is available,
*            the close and open switches the pipe to the lower file descriptor.
*            so we need to switch the listener.
* 2012-10-13 eat 0.8.12 Replace malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 Checker updates
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>

//=============================================================================
 
typedef struct NB_MOD_SERVER{
  struct IDENTITY *identity;      // identity
  char             idName[64];    // identity name
  char             filename[512]; // file name
  int              fildes;        // file descriptor
  char             buffer[NB_BUFSIZE];  // input buffer
  char            *cursor;        // location of next read
  unsigned char    ignore2eol;    // ignore to end of line
  unsigned char    trace;         // trace option
  } NB_MOD_Server;

//==================================================================================
//
// Handle connection requests
//
//   this routine replaces fifoAlertListener
//
void serverRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Server *server=handle;
  long len,used;
  //size_t size;
  char *bufcur,*bufeol;

  if(server->trace) nbLogMsg(context,0,'T',"serverRead: called");
  if(server->cursor<server->buffer){
    nbLogMsg(context,0,'L',"serverRead: server->cursor points outside of server->buffer - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  used=(server->cursor-server->buffer);
  if(used>=sizeof(server->buffer)){
    nbLogMsg(context,0,'L',"serverRead: server->cursor points outside of server->buffer - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  len=read(server->fildes,server->cursor,sizeof(server->buffer)-used);
  while(len==-1 && errno==EINTR){
    len=read(server->fildes,server->cursor,sizeof(server->buffer)-used);
    }
  if(len>0){
    len+=used;                                // adjust length for part we already had maybe
    server->cursor=server->buffer;            // assume next read will be fresh until we learn otherwise
    if(server->ignore2eol){
      nbLogMsg(context,0,'W',"Ignoring to end of line");
      if((bufeol=memchr(server->buffer,'\n',len))==NULL){
        *(server->buffer+len)=0;
        nbLogPut(context,"] %s\n",server->buffer);
        return;  // continue reading until end of line or end of file
        }
      *bufeol=0;                                                  // drop LF - 10
      if(bufeol>server->buffer && *(bufeol-1)==13) *(bufeol-1)=0;   // drop CR - 13
      nbLogPut(context,"] %s\n",server->buffer);
      server->ignore2eol=0;
      //bufcur=bufeol+1;                      // 2012-12-16 eat - no point in this.  we can just return
      //len-=bufeol+1-server->buffer;
      return;
      }
    else{
      nbLogMsg(context,0,'I',"FIFO %s@%s",server->idName,server->filename);
      bufcur=server->buffer;
      }
    while(len>0 && (bufeol=memchr(bufcur,'\n',len))!=NULL){
      *bufeol=0;                                           /* drop LF - 10 */
      if(bufeol>bufcur && *(bufeol-1)==13) *(bufeol-1)=0;  /* drop CR - 13 */
      if(server->trace) nbLogPut(context,"] %s\n",bufcur);
      nbCmdSid(context,bufcur,1,server->identity);
      nbLogFlush(context);
      len-=bufeol+1-bufcur;  
      if(len<0){
        nbLogMsg(context,0,'L',"serverRead: len has gone negative - this should never happen - terminating");
        exit(NB_EXITCODE_FAIL);
        }
      bufcur=bufeol+1;
      }
    if(len>0){
      if(len>=sizeof(server->buffer)){
        *(server->buffer+sizeof(server->buffer)-1)=0;
        nbLogMsg(context,0,'E',"Command fills %d character buffer before end of line - ignoring to end of line.",sizeof(server->buffer));
        nbLogPut(context,"] %s\n",server->buffer);
        server->ignore2eol=1;
        server->cursor=server->buffer;
        }
      else{
        strncpy(server->buffer,bufcur,len);
        server->cursor=server->buffer+len;  // next read is continuation of last command;
        *(server->cursor)=0;
        if(server->trace) nbLogPut(context,"Looking for more to go with: %s\n",server->buffer);
        }
      }
    }
  else{
    if(server->trace) nbLogMsg(context,0,'T',"serverRead: end of file reached");
    nbListenerRemove(context,server->fildes);
    close(server->fildes);
    server->fildes=0;
    if(server->cursor!=server->buffer){
      *(server->cursor)=0;
      nbLogPut(context,"] %s\n",server->buffer);
      nbLogMsg(context,0,'E',"Command ended without newline character ignored.");
      }
    server->cursor=server->buffer;
    server->ignore2eol=0;
#if defined(WIN32)
    if((server->fildes=open(server->filename,O_RDONLY))<0){
#else
    if((server->fildes=open(server->filename,O_RDONLY|O_NONBLOCK))<0){
#endif
      nbLogMsg(context,0,'E',"serverRead: unable to open FIFO %s (%d)",server->filename,server->fildes);
      }
    nbListenerAdd(context,server->fildes,server,serverRead);
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
  NB_MOD_Server *server;
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
  server=nbAlloc(sizeof(NB_MOD_Server));
  inCursor=server->idName;
  while(*cursor==' ') cursor++;
  while(*cursor && *cursor!='@'){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*cursor!='@'){
    nbLogMsg(context,0,'E',"Identity not found in pipe specification - expecting identity@filename");
    nbFree(server,sizeof(NB_MOD_Server));  // 2012-12-18 eat - CID 751617
    return(NULL);
    }
  cursor++;
  server->identity=nbIdentityGet(context,server->idName);
  if(server->identity==NULL){
    nbLogMsg(context,0,'E',"Identity '%s' not defined",server->idName);
    nbFree(server,sizeof(NB_MOD_Server));
    return(NULL);
    }
  inCursor=server->filename;
  while(*cursor){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*server->filename==0){
    nbLogMsg(context,0,'E',"File name not found in pipe specification - expecting identity@filename");
    nbFree(server,sizeof(NB_MOD_Server));
    return(NULL);
    }
  server->fildes=0;
  *server->buffer=0;
  server->cursor=server->buffer;
  server->ignore2eol=0;
  server->trace=0;
  nbCellDrop(context,cell);
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(server);
  }

/*
*  enable() method
*
*    enable <node>
*/
int serverEnable(nbCELL context,void *skillHandle,NB_MOD_Server *server){
#if defined(WIN32)
  if((server->fildes=open(server->filename,O_RDONLY))<0){
    // insert windows CreateNamedPipe here, unless we have to make separate funtions
#else
  if((server->fildes=open(server->filename,O_RDONLY|O_NONBLOCK))<0){
    if(mknod(server->filename,S_IFIFO|0600,0)<0){
      nbLogMsg(context,0,'E',"Unable to create FIFO %s - %s",server->filename,strerror(errno));
      return(1);
      }
#endif
#if defined(WIN32)
    if((server->fildes=open(server->filename,O_RDONLY))<0){
#else
    if((server->fildes=open(server->filename,O_RDONLY|O_NONBLOCK))<0){
#endif
      nbLogMsg(context,0,'E',"Unable to open FIFO %s - %s",server->filename,strerror(errno));
      return(1);
      }
    }
  nbListenerAdd(context,server->fildes,server,serverRead);
  nbLogMsg(context,0,'I',"Listening for FIFO connections as %s@%s",server->idName,server->filename);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int serverDisable(nbCELL context,void *skillHandle,NB_MOD_Server *server){
  nbListenerRemove(context,server->fildes);
  close(server->fildes);
  server->fildes=0;
  return(0);
  }


/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    <node>:trace,notrace
*/
int *serverCommand(nbCELL context,void *skillHandle,NB_MOD_Server *server,nbCELL arglist,char *text){
  if(strstr(text,"notrace")) server->trace=0;
  else if(strstr(text,"trace")) server->trace=1;
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
int serverDestroy(nbCELL context,void *skillHandle,NB_MOD_Server *server){
  if(server->trace) nbLogMsg(context,0,'T',"serverDestroy called");
  if(server->fildes!=0) serverDisable(context,skillHandle,server);
  nbFree(server,sizeof(NB_MOD_Server));
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

typedef struct NB_MOD_CLIENT{
  nbCELL  filenamecell;      // cell containing file name - drop on destroy
  char   *filename;          // file name
  int     fildes;            // file descriptor
  char    buffer[NB_BUFSIZE];      // input buffer
  } NB_MOD_Client;

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*
*    define <term> node pipe.reader("<identity>@<filename>");
*/
void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Client *client;
  nbCELL cell;
  nbSET argSet;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string pipe file name as first parameter");
    return(NULL);
    }
  client=(NB_MOD_Client *)nbAlloc(sizeof(NB_MOD_Client));
  client->filenamecell=cell;
  client->filename=nbCellGetString(context,cell);
  return(client);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int clientCommand(nbCELL context,void *skillHandle,NB_MOD_Client *client,nbCELL arglist,char *text){
  int wrote;
  size_t size;

  size=strlen(text)+1;
  if(size>=sizeof(client->buffer) || size>INT_MAX){
    nbLogMsg(context,0,'E',"Text may not exceed %zu characters",sizeof(client->buffer));
    return(-1);
    }
  snprintf(client->buffer,sizeof(client->buffer),"%s\n",text);
  client->fildes=open(client->filename,O_WRONLY|O_APPEND);
  if(client->fildes<0){ // 2012-12-27 eat 0.8.13 - CID 751565
    nbLogMsg(context,0,'E',"Unable to open %s for append - %s",client->filename,strerror(errno));
    return(-1);
    }
  wrote=write(client->fildes,client->buffer,size);
  close(client->fildes);
  if(wrote<0){
    nbLogMsg(context,0,'E',"Unable to write to pipe - %s",strerror(errno));
    return(-1);
    }
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
int clientDestroy(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  nbCellDrop(context,client->filenamecell);
  nbFree(client,sizeof(NB_MOD_Client));
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

