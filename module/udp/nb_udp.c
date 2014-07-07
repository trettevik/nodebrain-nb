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
*
* Name:   nb_udp.c
*
* Title:  NodeBrain UDP Command Exchange Module 
*
* Function:
*
*   This program is a NodeBrain node module for exchanging commands between
*   agents via UDP datagrams.  This mechanism does not provide for assured
*   delivery, so it should only be used in cases were lost commands can be
*   tolerated, or where delivery is assured via acknowledgement and resend
*   logic within the application.
*   
* Description:
*
*   This is an experimental UPD client and server node module.
*
*   You may define one or more server nodes, but only one can listen
*   to a give port on a given interface.
*
*     Syntax:
*
*       define <term> node udp.server("<identity>@<address>[:port]"[,"<prefix>"])[:<options>];
*
*       <identity>    - Identity under which the commands are executed
*       <address>     - Interface to bind to ("0.0.0.0" for all interfaces)
*       <port>        - UDP port to listen on
*       <prefix>      - Optional command prefix.  This may be used to send commands to 
*                       a different context, or to a command translator node or a special
*                       command handler node.
*       <options>     - Options to control the log output 
*
*                        trace   - display every trap received
*                        dump    - display a hex dump of UDP datagrams
*                        silent  - don't echo generated NodeBrain commands
*     Examples:
*
*       define udpserver node udp.server("default@0.0.0.0:49832"); 
*
*   Input packets are passed to the interpreter.
*
* Future:
*
*   This module will be upgraded to support client authentication with playback
*   protection.  For now we assume we are listing only to the loopback address
*   and that we trust the processes running on the local server.  This is not
*   a reasonable assumption.
*   
*===========================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2009/06/28 Ed Trettevik - original prototype version 0.7.6
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-13 eat 0.8.13 Checker updates
* 2013-01-16 eat 0.8.13 Checker updates
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>

/*
*  Datagram Format
*
*    The first byte of a datagram specifies the protocol version number.
*
*    Offset
*
*     0 - 0x00 (only version 0 is currently supported)
*     1 - command text
*
*/

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.  For example, serverRead() is an
*  example of a method that would only be used by a module that "listens".
*
*=================================================================================*/

//==================================================================================
// UDP Server

typedef struct NB_MOD_UDP_SERVER{  /* UDP server node descriptor */
  //struct IDENTITY *identity;     // identity associated with input commands
  char           *prefix;          // command prefix
  unsigned int   socket;           /* server socket for datagrams */
  char interfaceAddr[512];         /* interface address to bind listener */
  unsigned short port;             /* UDP port of listener */
  unsigned char  trace;            /* trace option */
  unsigned char  dump;             /* option to dump packets in trace */
  unsigned char  echo;             /* echo option */
  unsigned int   sourceAddr;       /* source address */
  } NB_MOD_Server;

/*
*  Read incoming packets
*
*    This function is designed to consume as many packets as
*    are available.  This means a UDP server node can dominate
*    other server nodes.
*/
static void serverRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Server *server=handle;
  char buffer[NB_BUFSIZE],*cursor;
  size_t buflen=NB_BUFSIZE;
  int  len;
  unsigned short rport;
  char daddr[40],raddr[40];
  int havedata=1;
  fd_set rfds;
  struct timeval tv;

  if(strlen(server->prefix)>256){
    nbLogMsg(context,0,'L',"serverRead: server prefix larger than 256 characters - %s",server->prefix);
    exit(NB_EXITCODE_FAIL);
    }
  while(havedata){
    snprintf(buffer,sizeof(buffer),"%s",server->prefix);
    cursor=buffer+strlen(buffer);
    len=nbIpGetDatagram(context,serverSocket,&server->sourceAddr,&rport,(unsigned char *)cursor,buflen-strlen(buffer));
    if(server->trace){
      nbIpGetSocketAddrString(serverSocket,daddr);
      nbLogMsg(context,0,'I',"Datagram %s:%5.5u -> %s len=%d",nbIpGetAddrString(raddr,server->sourceAddr),rport,daddr,len);
      }
    if(server->dump) nbLogDump(context,buffer,len);
    *(cursor+len)=0;  // make sure we have a null terminator
    *cursor=' ';      // replace version number with space character
    //nbCmdSid(context,buffer,1,server->identity);
    nbCmd(context,buffer,1);
    tv.tv_sec=0;
    tv.tv_usec=0;
    FD_ZERO(&rfds);
    FD_SET(serverSocket,&rfds);
    havedata=select(1,&rfds,NULL,NULL,&tv); 
    }
  }

/*
*  construct() method
*
*    define <term> node <skill>("<binding>"[,"prefix"])[:<text>]
*
*    <binding>    - "interface_address:port_number"
*    <prefix>     - optional command prefix
*    <text>       - flag keywords
*                     trace   - display input packets
*                     dump    - display dump of server packets
*                     silent  - don't echo generated NodeBrain commands 
*
*    define udpserver node udp.server("0.0.0.0:49832");
*    define udpserver node udp.server("0.0.0.0:49832","tranman:");
*    define udpserver node udp.server("0.0.0.0:49832"):dump,silent;
*/
static void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Server *server;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char interfaceAddr[sizeof(server->interfaceAddr)];
  unsigned int port=0;
  int type,trace=0,dump=0,echo=1;
  int len;
  char *str;
  char *prefix="";

  *interfaceAddr=0;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  delim=strchr(str,':');
  if(delim==NULL){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  len=delim-str;
  if(len>=sizeof(server->interfaceAddr)){
    nbLogMsg(context,0,'E',"Inteface IP address may not be greater than %d characters",sizeof(server->interfaceAddr)-1);
    nbCellDrop(context,cell);
    return(NULL);
    }
  strncpy(interfaceAddr,str,len);
  *(interfaceAddr+len)=0; 
  delim++;
  port=(unsigned int)atoi(delim);
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Second argument must be string - optional command prefix");
      return(NULL);
      }
    prefix=nbCellGetString(context,cell);
    if(strlen(prefix)>256){
      nbLogMsg(context,0,'E',"Prefix may not be creater than 256 - %s.",prefix);
      nbCellDrop(context,cell);
      return(NULL);
      }
    // we don't drop cell here because we want to preserve the prefix string for reference
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'E',"The server skill only accepts two argument.");
      nbCellDrop(context,cell);
      return(NULL);
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor); // 2013-01-14 eat - VID 955-0.8.13-3 FP but replaced strchr(cursor,0)
    saveDelim=*delim;
    *delim=0;
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    *delim=saveDelim;
    cursor=delim;
    if(*cursor==',') cursor++;
    while(*cursor==' ' || *cursor==',') cursor++;
    }
  server=nbAlloc(sizeof(NB_MOD_Server));
  server->prefix=prefix;
  server->socket=0;
  strcpy(server->interfaceAddr,interfaceAddr);
  server->port=port;
  server->trace=trace;
  server->dump=dump;
  server->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(server);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int serverEnable(nbCELL context,void *skillHandle,NB_MOD_Server *server){
  int fd;
  if((fd=nbIpGetUdpServerSocket(context,server->interfaceAddr,server->port))<0){  // 2012-12-27 eat 0.8.13 - CID 751574
    nbLogMsg(context,0,'E',"Unable to listen on port %s\n",server->port);
    return(1);
    }
  server->socket=fd;
  nbListenerAdd(context,server->socket,server,serverRead);
  nbLogMsg(context,0,'I',"Listening on UDP port %u for commands using prefix '%s'",server->port,server->prefix);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
static int serverDisable(nbCELL context,void *skillHandle,NB_MOD_Server *server){
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
static int *serverCommand(nbCELL context,void *skillHandle,NB_MOD_Server *server,nbCELL arglist,char *text){
  if(server->trace){
    nbLogMsg(context,0,'T',"nb_udp:serverCommand() text=[%s]\n",text);
    }
  /* insert command parsing code here */
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
static int serverDestroy(nbCELL context,void *skillHandle,NB_MOD_Server *server){
  nbLogMsg(context,0,'T',"serverDestroy called");
  if(server->socket!=0) serverDisable(context,skillHandle,server);
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

//======================================================================
// UDP Client

typedef struct NB_MOD_UDP_Client{  /* UDP server node descriptor */
  //struct IDENTITY *identity;     // client identity
  char           *prefix;          // command prefix
  unsigned int   socket;           /* client socket for datagrams */
  char address[16];          /* server address */
  unsigned short port;             /* server port */
  unsigned char  trace;            /* trace option */
  unsigned char  dump;             /* option to dump packets in trace */
  unsigned char  echo;             /* echo option */
  unsigned int   sourceAddr;       /* source address */
  } NB_MOD_Client;

static void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Client *client;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char serverAddr[512];
  unsigned int port=0;
  int type,trace=0,dump=0,echo=1;
  int len;
  char *str;
  char *prefix=NULL;

  *serverAddr=0;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  delim=strchr(str,':');
  if(delim==NULL){
    nbLogMsg(context,0,'E',"Expecting \"address:port\" as first argument");
    nbCellDrop(context,cell);
    return(NULL);
    }
  len=delim-str;
  // if(len>=sizeof(serverAddr)){ // 2012-12-16 eat - CID 751659
  if(len>=sizeof(client->address)){
    nbLogMsg(context,0,'E',"Inteface IP address may not be greater than %d characters",sizeof(serverAddr)-1);
    nbCellDrop(context,cell);
    return(NULL);
    }
  strncpy(serverAddr,str,len);
  *(serverAddr+len)=0;
  delim++;
  port=(unsigned int)atoi(delim);
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Second argument must be string - optional command prefix");
      return(NULL);
      }
    prefix=nbCellGetString(context,cell);
    // we don't drop cell here because we want to preserve the prefix string for reference
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'E',"The client skill only accepts two argument.");
      return(NULL);
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor);  // 2013-01-14 eat - VID 583-0.8.13-3 FP but replaced strchr(cursor,0)
    saveDelim=*delim;
    *delim=0;
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0;
    *delim=saveDelim;
    cursor=delim;
    if(*cursor==',') cursor++;
    while(*cursor==' ' || *cursor==',') cursor++;
    }
  client=nbAlloc(sizeof(NB_MOD_Client));
  client->prefix=prefix;
  client->socket=0;
  strncpy(client->address,serverAddr,sizeof(client->address)-1);
  *(client->address+sizeof(client->address)-1)=0;
  client->port=port;
  client->trace=trace;
  client->dump=dump;
  client->echo=echo;
  return(client);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int clientEnable(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  int fd;
  if(client->socket!=0) return(0);
  if((fd=nbIpGetUdpClientSocket(0,client->address,client->port))<0){ // 2012-12-27 eat 0.8.13 - CID 751573
    nbLogMsg(context,0,'E',"Unable to obtain client UDP socket %s:%d - %s",client->address,client->port,strerror(errno));
    return(1);
    }
  client->socket=fd;
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
static int clientDisable(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  if(client->socket==0) return(0);
#if defined(WIN32)
  closesocket(client->socket);
#else
  close(client->socket);
#endif
  client->socket=0;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *clientCommand(nbCELL context,void *skillHandle,NB_MOD_Client *client,nbCELL arglist,char *text){
  char buffer[NB_BUFSIZE],*cursor=buffer;
  int len,sent;

  if(client->trace){
    nbLogMsg(context,0,'T',"nb_udp:clientCommand() text=[%s]\n",text);
    }
  *buffer=0;  // version 0 transaction
  cursor++;
  if(client->prefix && *client->prefix){
    strcpy(cursor,client->prefix);
    cursor+=strlen(cursor);  // 2013-01-14 eat - VID 4102-0.8.13-3 FP but replace strchr(cursor,0) 
    }
  strcpy(cursor,text);
  cursor+=strlen(cursor);
  len=cursor-buffer;
  sent=send(client->socket,buffer,len,0);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
static int clientDestroy(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  nbLogMsg(context,0,'T',"clientDestroy called");
  if(client->socket!=0) clientDisable(context,skillHandle,client);
  nbFree(client,sizeof(NB_MOD_Client));
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

