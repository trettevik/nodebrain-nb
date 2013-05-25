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
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2012-02-07 dtl Checker updates
* 2012-05-20 eat 0.8.9  Merged client skill which had been a separate source file
* 2012-10-17 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-12 eat 0.8.13 Checker updates
* 2013-01-14 eat 0.8.13 Checker updates
*=============================================================================
*/
#include "config.h"
#include <nb/nb.h>

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

static int smtpPut(NB_IpChannel *channel){
  int sent;
  char *buffer=(char *)channel->buffer;

  //strcat(buffer,"\n");  // 2013-01-14 eat - VID 4300-0.8.13-3 - newline now included before call to this function
  sent=send(channel->socket,buffer,strlen(buffer),0);  // application must make sure this is not a sensitive data leak
  while(sent==-1 && errno==EINTR){
    sent=send(channel->socket,buffer,strlen(buffer),0);
    }
  return(sent);
  }

static int smtpGet(NB_IpChannel *channel){
  int len;
  char *cursor,*buffer=(char *)channel->buffer;

  if((len=recv(channel->socket,buffer,NB_BUFSIZE,0))<0) return(len); //buffer[NB_BUFSIZE] (not overflow)
  if(len<3 || len>=NB_BUFSIZE) return(-1); /* that's too big */
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
  char *buffer=(char *)channel->buffer; //buffer is sizeof NB_BUFSIZE
  char *line,*cursor;
  char fname[1024];           /* name of SMTP Listener queue directory */

  nbQueueGetNewFileName(fname,directory,0,'t');
  if((file=fopen(fname,"a"))==NULL){
    snprintf(buffer,(size_t)NB_BUFSIZE,"550 Sorry, unable to open %s\n",fname); // 2012-01-31 dtl: replased sprintf
    return(0);
    }
  fprintf(file,"From: %s %s\n",channel->ipaddr,clienthost);
  fprintf(file,"To: %s\n",user);
  fprintf(file,"- - - - - - - - - - - - - - - -\n");

  snprintf(buffer,(size_t)NB_BUFSIZE,"%s","354 Enter Mail, end with \".\" on a line by itself\n"); // 2012-01-31 dtl: replased strcpy 
  if((len=smtpPut(channel))<0){
    fclose(file);
    return(len);
    }

  while((len=recv(channel->socket,buffer,NB_BUFSIZE-1,0))>0){
    *(buffer+len)=0;
    line=buffer;
    while(line<buffer+len){
      cursor=strchr(line,10);               /* look for newline character */
      if(cursor==NULL) cursor=buffer+len;
      *cursor=0;      // discard newline
      if(*(cursor-1)==13) *(cursor-1)=0; // discard carriage return;
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

  sprintf(buffer,"421 anonymous NodeBrain SMTP Alert Server unavailable - too busy\n");
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
  char cmd[512];
  char mailaddress[341],*cursor;
  char *recipient;
  char clienthost[256];
  char hostname[256];
  //struct IDENTITY *identity=NULL;
  nbIDENTITY identity=NULL;
  nbCELL context=session->context;

  if(gethostname(hostname,sizeof(hostname))) strcpy(hostname,"anonymous");

  sprintf(buffer,"220 %s NodeBrain SMTP Alert Server Ready\n",hostname);
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
      strncpy(clienthost,cursor,sizeof(clienthost));
      *(clienthost+sizeof(clienthost)-1)=0;
      sprintf(buffer,"250 %s",hostname);
      }
    else if(strncmp(buffer,"QUIT",4)==0 || strncmp(buffer,"quit",4)==0){
      sprintf(buffer,"221 %s NodeBrain SMTP Alert Server closing connection\n",hostname);
      smtpPut(channel);
      state=0;
      }
    else if(strncmp(buffer,"MAIL FROM:",10)==0 || strncmp(buffer,"MAIL From:",10)==0){
      cursor=buffer+10;
      while(*cursor==' ') cursor++;
      strncpy(mailaddress,cursor,sizeof(mailaddress));
      *(mailaddress+sizeof(mailaddress)-1)=0;
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
        strncpy(mailaddress,cursor,sizeof(mailaddress));
        *(mailaddress+sizeof(mailaddress)-1)=0;
        sprintf(buffer,"250 %s... Recipient ok",mailaddress);
        if((cursor=strchr(mailaddress,'@'))!=NULL) *cursor=0;
        if((cursor=strchr(mailaddress,'>'))!=NULL) *cursor=0;
        if(*mailaddress=='<') recipient=mailaddress+1;
        else recipient=mailaddress;
        //if(*mailaddress!='<') snprintf(recipient,sizeof(recipient),"%s",mailaddress); //dtl: replaced strcpy
        //else snprintf(recipient,sizeof(recipient),"%s",mailaddress+1); //2012-01-31 dtl: replaced strcpy
	if((identity=nbIdentityGet(context,recipient))==NULL)
	  sprintf(buffer,"550 %s Unknown",recipient);
        }
      }
    else if(strncmp(buffer,"DATA",4)==0 || strncmp(buffer,"data",4)==0){
      char directory[1024];
      if(!identity) strcpy(buffer,"503 Need RCPT before DATA");
      else{
        snprintf(directory,sizeof(directory),"%s/%s",server->qDir,nbIdentityGetName(context,identity));
        if(smtpData(channel,clienthost,directory,nbIdentityGetName(context,identity))<0) state=0;
        }
      }
    else if(strncmp(buffer,"RSET",4)==0 || strncmp(buffer,"rset",4)==0){
      state=1;
      strcpy(buffer,"250 Reset state");
      }
    else if(strncmp(buffer,"VRFY",4)==0 || strncmp(buffer,"vrfy",4)==0){
      strcpy(buffer,"550 String does not match anything.");
      }
    else{
      strncpy(cmd,buffer,sizeof(cmd));
      *(cmd+sizeof(cmd)-1)=0;
      sprintf(buffer,"500 Command unrecognized: \"%s\"",cmd);
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
#define MSG_SIZE  1024   //msg has size at least 1024
nbServer *smtpServer(nbCELL context,char *cursor,char *qDir,char *msg){
  nbServer *server;
  char *inCursor;
  char *interfaceAddr;

  if(strlen(qDir)>=512){   // 2012-12-25 eat - AST 38 - replaced > with >=
    sprintf(msg,"Queue directory name too long for buffer");
    return(NULL);
    }
  server=(nbServer *)nbAlloc(sizeof(nbServer));
  snprintf(server->qDir,sizeof(server->qDir),"%s",qDir);  // 2013-01-17 eat - VID 6525 FP but changed from strcpy to snprintf
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
    nbFree(server,sizeof(nbServer));
    return(NULL);
    }
  cursor++;
  server->identity=nbIdentityGet(context,server->idName);
  if(server->identity==NULL){
    snprintf(msg,(size_t)MSG_SIZE,"Identity '%s' not defined",server->idName); //2012-01-31 dtl: replaced sprintf 
    nbFree(server,sizeof(nbServer));
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
    nbFree(server,sizeof(nbServer));
    return(NULL);
    }
  cursor++;
  inCursor=cursor;
  while(*cursor>='0' && *cursor<='9') cursor++;
  if(*cursor!=0){
    sprintf(msg,"Port not numeric in server specification - expecting identity@address:port");
    nbFree(server,sizeof(nbServer));
    return(NULL);
    }
  server->port=atoi(inCursor);
  server->socket=0;
  if(*server->address<'0' || *server->address>'9'){
    interfaceAddr=nbIpGetAddrByName(server->address);
    if(interfaceAddr==NULL){
      snprintf(msg,(size_t)MSG_SIZE,"Hostname %s not resolved",server->address); //2012-02-07 dtl: replaced sprintf
      nbFree(server,sizeof(nbServer));
      return(NULL);
      }
    strncpy(server->address,interfaceAddr,sizeof(server->address));
    *(server->address+sizeof(server->address)-1)=0;
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
  session=nbAlloc(sizeof(nbSession));
  channel=nbIpAlloc();  /* get a channel for a new thread */
  if(nbIpAccept(channel,(int)server->socket)<0){
    if(errno!=EINTR) nbLogMsg(context,0,'E',"smtpAccept: chaccept failed");
    else nbLogMsg(context,0,'E',"smtpAccept: chaccept interupted by signal.");
    nbIpFree(channel);
    return;
    }
  else{
    time(&now);
    if(now>=until){
      if(count>max) nbLogMsg(context,0,'I',"Rejected %d connections",count-max);
      count=0;
      until=now+1;
      }
    count++;
    if(count>max){
      smtpReject(channel); /* reject after the limit */
      return;
      }
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
  nbFree(server,sizeof(nbServer));
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
/*
* Copyright (C) 1998-2012 The Boeing Company
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
* File:     nb_mailer.c
*
* Title:    NodeBrain Mailer - SMTP Client
*
* Function:
*
*   This file provides the client skill of the mail module.
*
* Synopsis:
*
*   #include "nb.h"
*
* Description
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2012/03/16 Ed Trettevik - initial version 0.8.7
*            [This is heavily based on Cliff Bynum's Mailer]
*=============================================================================
*/

//#include <nb.h>

/*
*  The following structure is created by the skill module's "construct"
*  function defined in this file.  This is a module specific structure.
*  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_MAIL_CLIENT{      // mailer node descriptor
  unsigned char  trace;            // trace option
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  nbMailClient  *mailClient;    // Mail and socket information
  } nbModMailClient;

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node mail.client("<cabal>",<node>,<port>);
*    <term>. define filelines cell <filelines>; # number of lines per file
*/
void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModMailClient *client;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim;
  int trace=0,dump=0,echo=1;
  int len;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL) nbLogMsg(context,0,'E',"The client skill accepts no parameters - ignoring arguments.");
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(!delim) delim=strchr(cursor,',');
    if(!delim) delim=strchr(cursor,';');
    if(!delim) delim=cursor+strlen(cursor);  // 2012-12-27 eat 0.8.13 - CID 751553 - was checking for delim instead of !delim
    len=delim-cursor;
    if(strncmp(cursor,"trace",len)==0){trace=1;}
    else if(strncmp(cursor,"dump",len)==0){trace=1;dump=1;}
    else if(strncmp(cursor,"silent",len)==0) echo=0;
    cursor=delim;
    while(*cursor==' ' || *cursor==',') cursor++;
    }
  client=nbAlloc(sizeof(nbModMailClient));
  memset(client,0,sizeof(nbModMailClient));
  client->trace=trace;
  client->dump=dump;
  client->echo=echo;
  client->mailClient=NULL;
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(client);
  }

/*
*  enable() method
*
*    enable <node>
*/
int clientEnable(nbCELL context,void *skillHandle,nbModMailClient *client){
  if(!client->mailClient) client->mailClient=nbMailClientCreate(context);
  if(!client->mailClient){
    nbLogMsg(context,0,'E',"Unable to create mail client - terminating");
    exit(1);
    }
  nbLogMsg(context,0,'I',"Enabled");
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
int clientDisable(nbCELL context,void *skillHandle,nbModMailClient *client){
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *clientCommand(nbCELL context,void *skillHandle,nbModMailClient *client,nbCELL arglist,char *text){
  if(client->trace) nbLogMsg(context,0,'T',"clientCommand() text=[%s]\n",text);
  nbCmd(context,text,1);
  nbMailSendAlarm(context,client->mailClient);
  if(client->trace) nbLogMsg(context,0,'E',"clientCommand(): alarm sent");
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
int clientDestroy(nbCELL context,void *skillHandle,nbModMailClient *client){
  nbLogMsg(context,0,'T',"clientDestroy called");
  nbFree(client,sizeof(nbModMailClient));
  return(0);
  }


#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *clientBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,clientDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *mailerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,clientDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }
