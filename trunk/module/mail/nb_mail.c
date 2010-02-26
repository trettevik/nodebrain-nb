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
* File:     nbsmtp.c 
*
* Title:    NodeBrain SMTP Listener Routines [prototype]
*
* Function:
*
*   This file provides routines that implement NodeBrain's SMTP Listener.  This
*   is an "unauthenticated" method of passing information to a NodeBrain server. 
*
* Synopsis:
*
*   #include "nb.h"
*
* Description
*
*   The minimum command set specified in RFC 821 is faked by this
*   program to accept mail from an SMTP client.  This program may
*   not be compatible with all SMTP clients.
*
*     HELO
*     MAIL From:
*     RCPT To:
*     DATA
*     RSET
*     VRFY
*     QUIT
*
*   Non of the extensions specified in RFC 1869 are supported,
*   except we do respond to EHLO with a null list of extensions
*   to indicate that we only support the miminum set above.  This
*   is proper according to RFC 1869.  An alternative would be to
*   reply with a 500 or a 502.
*
* Exit Codes:
*
*   0 - Successful completion
*   1 - Error (see message)
* 
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/10/02 Ed Trettevik (prototype version introduced in 0.4.1 A5)
* 2002/11/25 eat - version 0.4.2 B2
*             1) Corrected error in the way the channels were managed by the
*                alert method on UNIX systems.
* 2003/03/03 eat 0.5.1  Conditional compile for Max OS X [ see defined(MACOS) ]
* 2004/10/06 eat 0.6.1  Conditionals for FreeBSD, OpenBSD, and NetBSD
* 2005/12/23 eat 0.6.4  Included calls to medulla interface in Enable/Disable methods
*            The long range plan is to convert this protocol into a skill module
*            and deprecate the old listener concept.
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
*=============================================================================
*/
#include "config.h"
#include <nb.h>
//#include <nbip.h>

void nbQueueCommit();

//=============================================================================

typedef struct NB_MOD_MAIL_SERVER{
  struct IDENTITY *identity;     // identity
  char             idName[64];   // identity name
  char             address[16];  // address to bind
  unsigned short   port;         // port to listen on
  int              socket;       // socket we are listening on
  char             qDir[512];    // queue directory
  } nbServer;

typedef struct NB_MOD_MAIL_SESSION{
  nbServer *server;
  nbCELL context;
  NB_IpChannel *channel;
  } nbSession;


//extern struct sigaction sigact;

int smtpPut(NB_IpChannel *channel){
  int sent;
  char *buffer=(char *)channel->buffer;
  strcat(buffer,"\n");
  sent=send(channel->socket,buffer,strlen(buffer),0);
  while(sent==-1 && errno==EINTR){
    sent=send(channel->socket,buffer,strlen(buffer),0);
    }
  return(sent);
  }

int smtpGet(NB_IpChannel *channel){
  int len;
  char *cursor,*buffer=(char *)channel->buffer;

  if((len=recv(channel->socket,buffer,NB_BUFSIZE,0))<0) return(len);
  if(len>4095) return(-1); /* that's too big */
  cursor=buffer+len;
  *cursor=0; cursor--;
  if(*cursor==10){*cursor=0; cursor--; len--;}
  if(*cursor==13){*cursor=0; cursor--; len--;}
  return(len);
  }

/*
*  Copy smtp data to a destination (file for now)
*
*  Returns:
*
*    1 - Message accepted for delivery
*    0 - Unable to complete delivery
*   -n - Unable to communicate with sender
*/
int smtpData(NB_IpChannel *channel,char *clienthost,char *directory,char *user){
  FILE *file;
  int len;
  char *buffer=(char *)channel->buffer;
  char *line,*cursor;
  char fname[1024];           /* name of SMTP Listener queue directory */


  nbQueueGetNewFileName(fname,directory,0,'t');
  if((file=fopen(fname,"a"))==NULL){
    sprintf(buffer,"550 Sorry, unable to open %s\n",fname);
    return(0);
    }
  fprintf(file,"From: %s %s\n",channel->ipaddr,clienthost);
  fprintf(file,"To: %s\n",user);
  fprintf(file,"- - - - - - - - - - - - - - - -\n");

  strcpy(buffer,"354 Enter Mail, end with \".\" on a line by itself");
  if((len=smtpPut(channel))<0) return(len);

  while((len=recv(channel->socket,buffer,NB_BUFSIZE,0))>0){
    *(buffer+len)=0;
    line=buffer;
    while(line<buffer+len){
      cursor=strchr(line,10);               /* look for newline character */
      if(cursor==NULL) cursor=buffer+len;
      *cursor=0;      // discard newline
      if(*(cursor-1)==13){
        cursor--;
        *cursor=0;    // discard carriage return
        }
      if(*line=='.' && *(line+1)==0){
        fclose(file);
        strcpy(buffer,"250 ... Message accepted for delivery");
        nbQueueCommit(fname);
        return(1);
        }
      fprintf(file,"%s\n",line);
      cursor++;
      line=cursor;
      }
    }
  fclose(file);
  strcpy(buffer,"250 ... Message accepted for delivery");
  nbQueueCommit(fname);
  return(1);
  }

/*
*  Reject a connection
*/
void smtpReject(NB_IpChannel *channel){
  char *buffer=(char *)channel->buffer;

  sprintf(buffer,"421 anonymous NodeBrain SMTP Alert Server unavailable - too busy");
  smtpPut(channel);
  nbIpClose(channel);
  nbIpFree(channel);
  }

/*
*  Serve a connection
*/
void smtpServe(nbSession *session){
  nbServer *server=session->server;
  NB_IpChannel *channel=session->channel;
  int len,state=1;
  char *buffer=(char *)channel->buffer;
  char mailaddress[4096],*cursor;
  char clienthost[256];
  char recipient[256];
  char hostname[256];
  //struct IDENTITY *identity=NULL;
  nbIDENTITY identity=NULL;
  nbCELL context=session->context;

  strcpy(hostname,"anonymous");  /* replace this with chgethost() */

  sprintf(buffer,"220 %s NodeBrain SMTP Alert Server Ready",hostname);
  while(state){
    /* send reply to client */
    if((len=smtpPut(channel))<0) state=0;
    /* get smtp command */
    else if((len=smtpGet(channel))<0) state=0;
    else if(len==0);
    //if(server->trace) nbLogMsg(context,0,'T',"smtp<:%s",buffer);
    //if(state==0);
    /* parse command and format a reply */
    else if(strncmp(buffer,"HELO",4)==0 || strncmp(buffer,"EHLO",4)==0){
      cursor=buffer+4;
      while(*cursor==' ') cursor++;
      strcpy(clienthost,cursor);
      sprintf(buffer,"250 %s",hostname);
      }
    else if(strncmp(buffer,"QUIT",4)==0 || strncmp(buffer,"quit",4)==0){
      sprintf(buffer,"221 %s NodeBrain SMTP Alert Server closing connection",hostname);
      smtpPut(channel);
      state=0;
      }
    else if(strncmp(buffer,"MAIL FROM:",10)==0 || strncmp(buffer,"MAIL From:",10)==0){
      cursor=buffer+10;
      while(*cursor==' ') cursor++;
      strcpy(mailaddress,cursor);
      sprintf(buffer,"250 %s... Sender ok",mailaddress);
      state=2;
      }
    else if(strncmp(buffer,"RCPT TO:",8)==0 || strncmp(buffer,"RCPT To:",8)==0){
      if(state<2){
        strcpy(buffer,"503 Need MAIL before RCPT");
        }
      else{
        cursor=buffer+8;
        while(*cursor==' ') cursor++;
        strcpy(mailaddress,cursor);
        sprintf(buffer,"250 %s... Recipient ok",mailaddress);
        if((cursor=strchr(mailaddress,'@'))!=NULL) *cursor=0;
        if((cursor=strchr(mailaddress,'>'))!=NULL) *cursor=0;
        if(*mailaddress!='<') strcpy(recipient,mailaddress);
        else strcpy(recipient,mailaddress+1);
	if((identity=nbIdentityGet(context,recipient))==NULL)
	  sprintf(buffer,"550 %s Unknown",recipient);
        }
      }
    else if(strncmp(buffer,"DATA",4)==0 || strncmp(buffer,"data",4)==0){
      char directory[1024];
      sprintf(directory,"%s/%s",server->qDir,nbIdentityGetName(context,identity));
      if(smtpData(channel,clienthost,directory,nbIdentityGetName(context,identity))<0) state=0;
      }
    else if(strncmp(buffer,"RSET",4)==0 || strncmp(buffer,"rset",4)==0){
      state=1;
      strcpy(buffer,"250 Reset state");
      }
    else if(strncmp(buffer,"VRFY",4)==0 || strncmp(buffer,"vrfy",4)==0){
      strcpy(buffer,"550 String does not match anything.");
      }
    else{
      strcpy(mailaddress,buffer);
      sprintf(buffer,"500 Command unrecognized: \"%s\"",mailaddress);
      }
    }
  nbIpClose(channel);
  nbIpFree(channel);
  return;
  }


#if defined(WIN32)
DWORD WINAPI ThreadFunc(nbSession *session){
  smtpServe(session);
  return(0);
  }

//void smtpFork(nbCELL context,NB_IpChannel *channel){
void smtpFork(nbCELL context,nbSession *session){
  DWORD dwThreadId;
  HANDLE hThread;

  hThread = CreateThread(
    NULL,                        // no security attributes
    64000,                       // use default stack size
    (LPTHREAD_START_ROUTINE)ThreadFunc, // thread function
    session,                     // argument to thread function
//    channel,                     // argument to thread function
    0,                           // use default creation flags
    &dwThreadId);                // returns the thread identifier

  // Check the return value for success.

  if(hThread==NULL) nbLogMsg(context,0,'E',"smtpFork() CreateThread failed");
  else CloseHandle(hThread);
  }
#else
/*
*  Fork a child process to serve a connection
*/
void smtpFork(nbCELL context,nbSession *session){
  int pid;

#if defined(mpe) || defined(ANYBSD)
  if((pid=fork())<0){
#else
// 2005-12-12 eat 0.6.4 - handling SIGCLD differently
//  sigaction(SIGCLD,NULL,&sigact);
//  sigact.sa_handler=SIG_IGN;
///*  SA_NOCLDWAIT required on Solaris 2.5.1 but not defined on Linux */
//#if defined(SOLARIS)
//  sigact.sa_flags=SA_NOCLDSTOP+SA_NOCLDWAIT;
//#else
//  sigact.sa_flags=SA_NOCLDSTOP;
//#endif
//  sigaction(SIGCLD,&sigact,NULL);
  if((pid=vfork())<0){
#endif
    nbLogMsg(context,0,'E',"smtpFork() Unable to create child process");
    return;
    }
  if(pid>0) return;
  else{
    NB_IpChannel *newChannel;
    newChannel=nbIpAlloc();
    newChannel->socket=session->channel->socket;
    strcpy(newChannel->ipaddr,session->channel->ipaddr);
    session->channel=newChannel;
    session->context=context;
    smtpServe(session);
    }
  _exit(0);
  }
#endif

/*
* Copyright (C) 1998-2007 The Boeing Company
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
* Name:   nb_mail.c
*
* Title:  NodeBrain Mail Reader Skill Module 
*
* Function:
*
*   This program is a NodeBrain skill module that accepts mail and
*   stores it in a queue for handling by a Peer node.
*   
* Description:
* 
*   This module is for use only when better options are not available.
*   It is a primative prototype module based on a capability once
*   built-in to the NodeBrain interpreter.  Most sensor products
*   have the ability to send mail notification in response to detected
*   conditions.  The mail module does not currently provide that
*   function to NodeBrain.  For that purpose, we rely on servant
*   scripts.  Instead, the mail module collects mail from sensor
*   products in situations where no better interface exists.  It has
*   been used to collect mail on the local host with delivery via the
*   Peer module, to avoid sending plain text mail over the network.  
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2007/06/23 Ed Trettevik - original skill module prototype version
* 2007/06/23 eat 0.6.8  Structured skill module around old SMTP listener code
*=====================================================================
*/

//==================================================================================
// Create new server structure from server specification
//
//    identity@address:port

nbServer *smtpServer(nbCELL context,char *cursor,char *qDir,char *msg){
  nbServer *server;
  char *inCursor;
  char *interfaceAddr;

  if(strlen(qDir)>512){
    sprintf(msg,"Queue directory name too long for buffer");
    return(NULL);
    }
  server=malloc(sizeof(nbServer));
  strcpy(server->qDir,qDir);
  inCursor=server->idName;
  while(*cursor==' ') cursor++;
  while(*cursor && *cursor!='@'){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*cursor!='@'){
    sprintf(msg,"Identity not found in server specification - expecting identity@address:port");
    free(server);
    return(NULL);
    }
  cursor++;
  server->identity=nbIdentityGet(context,server->idName);
  if(server->identity==NULL){
    sprintf(msg,"Identity '%s' not defined",server->idName);
    free(server);
    return(NULL);
    }
  inCursor=server->address; 
  while(*cursor && *cursor!=':'){
    *inCursor=*cursor; 
    inCursor++;
    cursor++;
    } 
  *inCursor=0;
  if(*cursor!=':'){
    sprintf(msg,"Address not found in server specification - expecting identity@address:port");
    free(server);
    return(NULL);
    }
  *cursor++;
  inCursor=cursor;
  while(*cursor>='0' && *cursor<='9') cursor++;
  if(*cursor!=0){
    sprintf(msg,"Port not numeric in server specification - expecting identity@address:port");
    free(server);
    return(NULL);
    }
  server->port=atoi(inCursor);
  server->socket=0;
  if(*server->address<'0' || *server->address>'9'){
    interfaceAddr=nbIpGetAddrByName(server->address);
    if(interfaceAddr==NULL){
      sprintf(msg,"Hostname %s not resolved",server->address);
      free(server);
      return(NULL);
      }
    strcpy(server->address,interfaceAddr);
    }
  return(server); 
  }

//==================================================================================
//
// Handle connection requests
//
void smtpAccept(nbCELL context,int serverSocket,void *handle){
  nbServer *server=handle;
  NB_IpChannel *channel;
  static time_t now,until=0;
  static long count=0,max=10;  /* accept 10 connections per second */
  nbSession *session;
  session=malloc(sizeof(nbSession));
  channel=nbIpAlloc();  /* get a channel for a new thread */
  if(nbIpAccept(channel,(int)server->socket)<0){
    if(errno!=EINTR){
      nbLogMsg(context,0,'E',"smtpAccept: chaccept failed");
      return;
      }
    nbLogMsg(context,0,'E',"smtpAccept: chaccept interupted by signal.");
    nbIpFree(channel);
    }
  else{
    time(&now);
    if(now>=until){
      if(count>max) nbLogMsg(context,0,'I',"Rejected %d connections",count-max);
      count=0;
      until=now+1;
      }
    count++;
    if(count>max) smtpReject(channel); /* reject after the limit */
    else{
      nbLogMsg(context,0,'I',"Request on port %s:%d from %s",server->address,server->port,channel->ipaddr);
      session->server=server;
      session->channel=channel;
      smtpFork(context,session);
      }
    }
#if !defined(WIN32)
  nbIpClose(channel);
  nbIpFree(channel);
#endif
  }

/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define mailbox node mail.reader("<identity>@<address>:port");
*/
void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbServer *server;
  nbCELL cell=NULL,qCell=NULL;
  nbSET argSet;
  char *serverSpec,*qDir;
  char msg[1024];

  //nbLogMsg(context,0,'T',"serverConstruct: called");
  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string server specification as first parameter - identity@address:port");
    return(NULL);
    }
  serverSpec=nbCellGetString(context,cell);
  qCell=nbListGetCellValue(context,&argSet);
  if(qCell==NULL || nbCellGetType(context,qCell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string queue directory as second parameter.");
    return(NULL);
    }
  qDir=nbCellGetString(context,qCell);
  server=smtpServer(context,serverSpec,qDir,msg);
  if(server==NULL){
    nbLogMsg(context,0,'E',msg);
    return(NULL);
    }
  nbCellDrop(context,cell);
  nbCellDrop(context,qCell);
  
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  //nbLogMsg(context,0,'T',"serverConstruct: returning");
  return(server);
  }

/*
*  enable() method
*
*    enable <node>
*/
int serverEnable(nbCELL context,void *skillHandle,nbServer *server){
  // we should use an API function here
  if((server->socket=nbIpListen(server->address,server->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on %s:%u",server->address,server->port);
    return(1);
    }
  nbListenerAdd(context,server->socket,server,smtpAccept);
  nbLogMsg(context,0,'I',"Listening for SMTP connections as %s@%s:%u",server->idName,server->address,server->port);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int serverDisable(nbCELL context,void *skillHandle,nbServer *server){
  nbListenerRemove(context,server->socket);
#if defined(WIN32)
  closesocket(server->socket);
#else
  close(server->socket);
#endif
  server->socket=0;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *serverCommand(nbCELL context,void *skillHandle,nbServer *server,nbCELL arglist,char *text){
  /* process commands here */
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
int serverDestroy(nbCELL context,void *skillHandle,nbServer *server){
  nbLogMsg(context,0,'T',"serverDestroy called");
  if(server->socket!=0) serverDisable(context,skillHandle,server);
  free(server);
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
