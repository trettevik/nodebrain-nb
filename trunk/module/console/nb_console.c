/*
* Copyright (C) 2004-2012 The Boeing Company
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
*=========================================================================
*
* Name:   nb_console.c
*
* Title:  NodeBrain Console Node Module 
*
* Function:
*
*   This program is a NodeBrain module that provides support to
*   the NodeBrain Console (Java).  This module is intended for brains
*   providing a console service listener, and console skulls spawned
*   by a console service listener.
*   
* Description:
*
*   When a console node is enabled, it listeners for connection
*   requests from users of the NodeBrain Console.  The intial prototype
*   version provides no authentication or encryption, so it is highly
*   recommended that you run your listener on the user's workstation
*   and configure it to only listen to localhost.
*
*   Once a connection is established it is managed as a user session.
*   Multiple sessions may be established and managed concurrently.
*
*   The primary services provided by this modules are:
*
*     1) An interface to NodeBrain command execution.
*     2) Access to NodeBrain internal data (rules and state).
*     3) Access to rule files.  This enables agent configuration via
*        the NodeBrain Console. 
*             
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2004-11-20 Ed Trettevik - original prototype version
* 2005-04-09 eat 0.6.2  adapted to API changes
* 2005-05-14 eat 0.6.3  consoleBind() modified to accept moduleHandle
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2012-01-31 dtl 0.8.10 Checker updates
* 2012-10-31 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=====================================================================
*/
#include "config.h"
#include <nb.h>

typedef struct NB_MOD_CONSOLE{
  unsigned short port;            /* TCP port of listener */
  char           dirname[256];    /* configuration directory name */
  int            serverSocket;    /* server Socket */
  struct NB_MOD_CONSOLE_SESSION *sessions;  /* list of sessions */
  char           trace;
  } NB_MOD_Console;

typedef struct NB_MOD_CONSOLE_SESSION{
  struct NB_MOD_CONSOLE_SESSION *next; /* next session */
  struct NB_MOD_CONSOLE *console;      /* console structure */
  NB_IpChannel          *channel;      /* communcations channel */
  char                  dirname[25];   /* session directory name */
  } NB_MOD_ConsoleSession;

/*================================================================================*/
void consoleOutputHandler(nbCELL context,void *session,char *buffer){
  nbIpPut(((NB_MOD_ConsoleSession *)session)->channel,buffer,strlen(buffer));
  }

void consoleStreamHandler(nbCELL context,void *session,char *buffer){
  nbIpPutMsg(((NB_MOD_ConsoleSession *)session)->channel,buffer,strlen(buffer));
  }

/*
*  Handle console commands
*/
void consoleCmdHandler(nbCELL context,NB_MOD_ConsoleSession *session,char *cursor){
  char verb[256],*curverb=verb;

  while(*cursor==' ') cursor++;
  while(*cursor!=' ' && *cursor!=0){
    *curverb=*cursor;
    curverb++;
    cursor++;
    }
  *curverb=0;
  while(*cursor==' ') cursor++; 
  if(strcmp(verb,"open")==0){
    if(!nbStreamOpen(context,cursor,session,consoleStreamHandler))
      nbLogMsg(context,0,'T',"stream \"%s\" not found",cursor);
    else{
      nbLogMsg(context,0,'T',"subscription to stream \"%s\" opened",cursor);
      }
    }
  else if(strcmp(verb,"close")==0){
    if(!nbStreamClose(context,cursor,session,consoleStreamHandler))
      nbLogMsg(context,0,'T',"stream \"%s\" not found",cursor);
    else{
      nbLogMsg(context,0,'T',"subscription to stream \"%s\" closed",cursor);
      }
    }
  }

/*
*  Service a console conversation 
*/
void consoleService(nbCELL context,int socket,void *handle){
  NB_MOD_ConsoleSession *session=handle;
  char buffer[NB_BUFSIZE];
  int  len;
  NB_IpChannel *channel=session->channel;

  nbLogMsg(context,0,'T',"Servicing console request");
  if((len=nbIpGet(channel,buffer))<=0){
    nbIpClose(channel);
    nbListenerRemove(context,socket);
    nbStreamClose(context,NULL,session,consoleStreamHandler);
    nbFree(session,sizeof(struct NB_MOD_CONSOLE_SESSION));
    }
  nbLogMsg(context,0,'T',"Request length=%d\n",len);
  if(len==0) return;
  nbLogHandlerAdd(context,session,consoleOutputHandler);
  /* temporary commands for consoleService routine */
  if(*buffer=='+' && *(buffer+1)==':') consoleCmdHandler(context,session,buffer+2);
  else nbCmd(context,buffer,1);
  nbLogHandlerRemove(context,session,consoleOutputHandler);
  nbIpStop(session->channel);
  }

/*
*  Accept console session requests
*/
void consoleAccept(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Console *console=handle;
  NB_IpChannel *channel;
  NB_MOD_ConsoleSession *session;

  channel=nbIpAlloc();
  if(nbIpAccept(channel,serverSocket)<0){
    if(errno!=EINTR){
      nbLogMsg(context,0,'E',"nb_console:consoleAccept(): chaccept failed");
      nbIpFree(channel);
      }
    if(console->trace) nbLogMsg(context,0,'T',"nb_console:consoleAccept(): chaccept interupted by signal.");
    }

  /* here's where we need to do authentication */
  /* only listen to local host on user workstation until we beef this up */

  session=nbAlloc(sizeof(struct NB_MOD_CONSOLE_SESSION));
  session->next=console->sessions;
  console->sessions=session; 
  session->console=console;
  session->channel=channel;
  *session->dirname=0;

  /* listen for conversations */
  nbListenerAdd(context,channel->socket,session,consoleService);
  nbLogMsg(context,0,'I',"Console session established on socket %u\n",channel->socket);
  }



/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define console node console(<port>,<directory>);
*/
void *consoleConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Console *console;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim=0;
  unsigned int d,port;
  double r;
  int trace=0;
  char *dirnameP,dirname[256];

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(!cell || nbCellGetType(context,cell)!=NB_TYPE_REAL){
    nbLogMsg(context,0,'E',"Expecting numeric TCP port number as first argument");
    return(NULL);
    }
  r=nbCellGetReal(context,cell);
  nbCellDrop(context,cell);
  port=(unsigned int)r;
  d=port;
  if(d!=r || d==0){
    nbLogMsg(context,0,'E',"Expecting non-zero integer TCP port number as first argument");
    return(NULL);
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Expecting string argument for directory name");
      return(NULL);
      }
    dirnameP=nbCellGetString(context,cell);
    strncpy(dirname,dirnameP,255);
    nbCellDrop(context,cell);
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL) nbLogMsg(context,0,'W',"Unexpected argument - third argument and above ignored");
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=strchr(cursor,0);
    saveDelim=*delim;
    *delim=0;
    if(strcmp(cursor,"trace")==0) trace=1; 
    *delim=saveDelim;
    cursor=delim;
    if(*cursor==',') cursor++;
    while(*cursor==' ') cursor++;
    }
  console=nbAlloc(sizeof(NB_MOD_Console));
  console->sessions=NULL;
  console->serverSocket=0;
  console->port=port;
  strncpy(console->dirname,dirname,255); 
  *(console->dirname+255)=0; //2012-01-31 dtl: added string terminator
  console->trace=trace;
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(console);
  }

/*
*  enable() method
*
*    enable <node>
*/
int consoleEnable(nbCELL context,void *skillHandle,NB_MOD_Console *console){
  if((console->serverSocket=nbIpListen("0.0.0.0",console->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on port %s\n",console->port);
    return(1);
    }
  nbListenerAdd(context,console->serverSocket,console,consoleAccept);
  nbLogMsg(context,0,'I',"Listening on port %u for console connections\n",console->port);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int consoleDisable(nbCELL context,void *skillHandle,NB_MOD_Console *console){
  NB_MOD_ConsoleSession *session;
  nbListenerRemove(context,console->serverSocket);
  nbIpCloseSocket(console->serverSocket);
  console->serverSocket=0;
  session=console->sessions;
  console->sessions=NULL;
  while(session!=NULL){
    /* we should send a message to console here */
    /* for now we just close the session */
    nbIpClose(session->channel);
    nbIpFree(session->channel);
    session=session->next;
    nbFree(session,sizeof(struct NB_MOD_CONSOLE_SESSION));
    }
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    <node>:trace
*/
int *consoleCommand(nbCELL context,void *skillHandle,NB_MOD_Console *console,nbCELL arglist,char *text){
  if(console->trace){
    nbLogMsg(context,0,'T',"nb_console:consoleCommand() text=[%s]\n",text);
    }
  /* process commands here */
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int consoleDestroy(nbCELL context,void *skillHandle,NB_MOD_Console *console){
  nbLogMsg(context,0,'T',"consoleDestroy called");
  if(console->serverSocket!=0) consoleDisable(context,skillHandle,console);
  nbFree(console,sizeof(NB_MOD_Console));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *consoleBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,consoleConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,consoleDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,consoleEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,consoleCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,consoleDestroy);
  return(NULL);
  }
