/*
* Copyright (C) 2005-2014 Ed Trettevik <eat@nodebrain.org>
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
* Name:   nb_syslog.c
*
* Title:  Remote Syslog Monitor 
*
* Function:
*
*   This program is a NodeBrain skill module for logging local syslog
*   messages, sending syslog messages to remote servers, and monitoring
*   syslog messages from remote servers.
*   
* Reference:    RFC 3164 - "The BSD syslog Protocol"
*               RFC 5424 - "The Syslog Protocol"
*
* See also:     logger, syslogd, syslog-ng
*
* Description:
*
*   This module provides an interface between NodeBrain and the system
*   logging facility found on UNIX and Linux servers.  There are three
*   node skills provided by this module:
*
*     logger - send message to the local syslog facility
*     client - send message to a remote syslog facility
*     server - receive and process messages via the syslog protocol
*
*   The logger skill provides a simple method of logging syslog messages
*   without having to invoke the shell to run the logger command.
*
*     Syntax:
*
*       define <term> node syslog.logger[("<ident>")][:<options>];
*
*       <ident>  - identifier to be included in the message
*                  <date> <host> <ident>[<pid>] <message>
*                  Default is "nodebrain"
*                  
*       <term>:<message>  # log a message
*
*     Example:
*
*       define log node syslog.logger;
*
*       log:This is an example
*     
*       Feb 26 07:48:52 myhostname nodebrain[6770]: This is an example
*
*   The client skill is like the logger skill, except messages are sent to
*   remote syslog servers directly.  Using the logger skill you can configure
*   the local syslogd or syslog-ng to forward messages to remote syslog
*   servers.  The client skill only adds value when you want to export messages
*   without depending on a functioning syslog daemon.  This should be rare,
*   but one example might be to report a tampering with syslog daemon 
*   configuration.
*
*     Syntax:
*
*       define <term> node syslog.client("<ident>","<uri>")[:<options>];
*
*       <ident>  - same as logger above
*
*       <uri>    - syslog server specifications
*
*                  udp://<hostname>[:<port>],...
*
*   The server skill provides a syslog monitoring capability, but not a
*   log management capability, because received messages are not stored.
*   Although the server skill can be used to accept syslog on the standard
*   port (UDP 514), this is not recommended.  Instead, you should generally
*   reserve UDP 514 for a standard syslog server (syslogd or syslog-ng).
*   A NodeBrain syslog server node provides a monitoring service in addition
*   to whatever log management solution you select.
*
*   It is convenient to combine NodeBrain syslog server nodes with syslog-ng,
*   because syslog-ng can take care of routing messages to the appropriate log
*   files, and routing of messages to appropriate monitoring nodes.  This is
*   done by configuring syslog-ng to forward to local domain sockets for
*   monitoring by syslog server nodes.
*   
*   You may define one or more syslog nodes, but only one can listen
*   to a give socket.
*
*     Syntax:
*
*       define <term> node syslog("translator"[,<binding>)][:<options>];
*
*       <translator>  -  Bind to a specific interface and/or port
*       <binding>     -  Bind to a specific interface and/or port
*                   
*                        "<address>"       - bind to interface
*                                            (default is all interfaces)
*                        <port>            - bind alternate port number
*                                            (default is 514)
*                        "<address>:port"  - bind to interface and port
*
*       <options>     -  Options to control the log output 
*
*                        trace   - display every trap received
*                        dump    - display a hex dump of UDP datagrams
*                        silent  - don't echo generated NodeBrain commands
*     Examples:
*
*       define syslog node syslog.server("syslog.nbx"); 
*       define syslog node syslog.server("syslog.nbx","127.0.0.1"); # bind to local host 
*       define syslog node syslog.server("syslog.nbx",50514);  # alternate port
*       define syslog node syslog.server("syslog.nbx","127.0.0.1:50514");  # both
*       define syslog node syslog.server("syslog.nbx"):dump;  # specify an option
*       define syslog node syslog.server("syslog.nbx",50514):silent; # specify port and option
*
*   All input packets are passed to the translator.  It is the translator's job to
*   match lines of syslog text to regular expressions and issue NodeBrain commands
*   to the node context---typically ASSERT or ALERT commands.  Rules in the node
*   context determine how to respond to alerts and assertions.
*
*===========================================================================
* NodeBrain API Functions Used:
*
*   Here's an outline of the API functions used by this module.  Refer to
*   the online NodeBrain User's Guide for a description of the NodeBrain
*   Skill Module API.  However, you can probably figure it out by
*   the example of this module using this outline as a starting point.
*   
*   Inteface to NodeBrain skill modules
*
*     nbSkillSetMethod()  - Used to tell NodeBrain about our methods.  
*
*       serverConstruct()  - construct an syslog node
*       serverEnable()     - start listening
*       serverDisable()    - stop listening
*       serverCommand()    - handle a custom command
*       serverDestroy()    - clean up when the node is destroyed
*
*   Interface to NodeBrain objects (primarily used in serverConstruct)
*
*     nbCellCreate()      - Create a new cell
*     nbCellGetType()     - Identify the type of a cell
*     nbCellGetString()   - Get a C string from a string cell
*     nbCellGetReal()     - Get a C double value from a real cell
*     nbCellDrop()        - Release a cell (decrement use count)
*
*   Interface to NodeBrain actions
*
*     nbLogMsg()          - write a message to the log
*     nbLogDump()         - write a buffer dump to the log
*     nbCmd()             - Issue a NodeBrain command
*
*   Interface to NodeBrain translators
*
*     nbTranslatorCompile  - Load a translator configuration file
*     nbTranslatorExecute  - Translate a string into NodeBrain commands
*
*   Interface to NodeBrain listeners
*
*     nbListenerAdd()     - Start listening to file/socket
*
*       serverRead()      - Handler passed to nbListenerAdd()   
*
*     nbListenerRemove()  - Stop listening to file/socket
*
*   Interface to Internet Protocol (IP) provided by NodeBrain
*
*     These functions are not an interface to NodeBrain, but are provided
*     by NodeBrain to symplify some common tasks for socket programming.  If you
*     use this module as a model for another, you may replace these functions
*     with your own direct calls to C functions for socket programming.  In other
*     words, NodeBrain doesn't care if you use these or not. 
*
*     nbIpGetUdpServerSocket()  - Obtain a UDP server socket
*     nbIpGetDatagram()         - Read a UDP packet (datagram)
*     nbIpGetSocketAddrString() - Get address of interface we are listening on
*     nbIpGetAddrStr()          - Convert an IP address from internal form to string
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2005-03-24 Ed Trettevik - original prototype version 0.6.2
* 2005-03-27 eat 0.6.2  Added comments for reference as a sample module
* 2005-04-09 eat 0.6.2  adapted to API changes
* 2005-05-14 eat 0.6.3  syslogBind() updated for moduleHandle
* 2006-10-30 eat 0.6.6  Modified to use NodeBrain Translator
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2011-02-23 eat 0.8.5  Included logger skill
* 2011-02-26 eat 0.8.5  Included client skill
* 2011-02-26 eat 0.8.5  Modified server skill to support local domain sockets
* 2012-10-17 eat 0.8.12 Checker updates
* 2012-10-18 eat 0.8.12 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>
#include <syslog.h>

/*
*  The following structure is created by the skill module's "construct"
*  function (serverContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_SERVER{      /* syslog.server node descriptor */
  char          *uri;              /* uri of socket we listen on */
  unsigned int   socket;           /* server socket for datagrams */
  char interfaceAddr[512];          /* interface address to bind listener */
  unsigned short port;             /* UDP port of listener */
  struct NB_CELL *translator;      /* syslog message text translator */
  unsigned char  trace;            /* trace option */
  unsigned char  dump;             /* option to dump packets in trace */
  unsigned char  echo;             /* echo option */
  unsigned int   sourceAddr;       /* source address */
  } NB_MOD_Server;

/*================================================================================*/

/*
*  syslog Datagram Format (See RFC 3164)
*
*  Example 1:
*
*    0000 3c37383e 2f555352 2f534249 4e2f4352 <78>/USR/SBIN/CR
*    0010 4f4e5b32 31363435 5d3a2028 726f6f74 ON[21645]:.(root
*    0020 2920434d 4420282f 6f70742f 7379736d ).CMD.(/opt/sysm
*    0030 6f6e2f62 696e2f73 6d6b6167 656e7420 on/bin/smkagent.
*    0040 63686563 6b203e20 2f646576 2f6e756c check.>./dev/nul
*    0050 6c20323e 26312920 0a...... ........ l.2>&1).. 
*
*  <n> -  Facility and Severity (if not found use <13>)
*
*      Facility = n/8
*      Severity = n%8
*
*      Numerical         Severity
*         Code
*
*          0       Emergency: system is unusable
*          1       Alert: action must be taken immediately
*          2       Critical: critical conditions
*          3       Error: error conditions
*          4       Warning: warning conditions
*          5       Notice: normal but significant condition
*          6       Informational: informational messages
*          7       Debug: debug-level messages
*
*      Numerical             Facility
*         Code
*
*          0             kernel messages
*          1             user-level messages
*          2             mail system
*          3             system daemons
*          4             security/authorization messages (note 1)
*          5             messages generated internally by syslogd
*          6             line printer subsystem
*          7             network news subsystem
*          8             UUCP subsystem
*          9             clock daemon (note 2)
*         10             security/authorization messages (note 1)
*         11             FTP daemon
*         12             NTP subsystem
*         13             log audit (note 1)
*         14             log alert (note 1)
*         15             clock daemon (note 2)
*         16             local use 0  (local0)
*         17             local use 1  (local1)
*         18             local use 2  (local2)
*         19             local use 3  (local3)
*         20             local use 4  (local4)
*         21             local use 5  (local5)
*         22             local use 6  (local6)
*         23             local use 7  (local7)
*
*  timestamp - optional time stamp "Jan dd hh:mm:ss" 
*
*  hostname  - optional hostname or IP address
*
*  message   - message text
*
*              process[pid]: ...text...
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

/*
*  Read incoming packets
*/
void serverRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Server *server=handle;
  char buffer[NB_BUFSIZE];
  size_t buflen=NB_BUFSIZE;
  int  len;
  unsigned short rport;
  char daddr[40],raddr[40];

  nbIpGetSocketAddrString(serverSocket,daddr);
  len=nbIpGetDatagram(context,serverSocket,&server->sourceAddr,&rport,(unsigned char *)buffer,buflen);
  while(len<0 && errno==EINTR) len=nbIpGetDatagram(context,serverSocket,&server->sourceAddr,&rport,(unsigned char *)buffer,buflen);
  if(len<0) return;  // 2012-12-18 eat - CID 751566
  if(server->trace) nbLogMsg(context,0,'I',"Datagram %s:%5.5u -> %s len=%d",nbIpGetAddrString(raddr,server->sourceAddr),rport,daddr,len);
  if(server->dump) nbLogDump(context,buffer,len);
  *(buffer+len)=0;  // make sure we have a null terminator
  nbTranslatorExecute(context,server->translator,buffer);
  }

/*
*  construct() method
*
*    define <term> node <skill>("<translator>",[<binding>])[:<text>]
*
*    <translator> - name of translator file
*    <binding>    - port_number or "interface_address[:port_number]"
*    <text>       - flag keywords
*                     trace   - display input packets
*                     dump    - display dump of syslog packets
*                     silent  - don't echo generated NodeBrain commands 
*
*    define syslog node syslog.server("syslog.nbx");
*    define syslog node syslog.server("syslog.nbx"):dump,silent;
*    define syslog node syslog.server("syslog.nbx","127.0.0.1");
*    define syslog node syslog.server("syslog.nbx",50162);
*    define syslog node syslog.server("syslog.nbx","127.0.0.1:50162");
*    define syslog node syslog.server("syslog.nbx","127.0.0.1:50162"):silent;
*/
void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Server *server;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  double r,d;
  char interfaceAddr[512];
  unsigned int port=514;
  int type,trace=0,dump=0,echo=1;
  int len;
  char *str;
  char *transfilename;
  nbCELL translator;
  char *uri="";;

  *interfaceAddr=0;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Translator configuration file required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying translator configuration file");
    return(NULL);
    }
  transfilename=nbCellGetString(context,cell);
  translator=nbTranslatorCompile(context,0,transfilename);
  if(translator==NULL){
    nbLogMsg(context,0,'E',"Unable to load translator '%s'",transfilename);
    return(NULL);
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_STRING){
      str=nbCellGetString(context,cell);
      uri=strdup(str);
      if(!uri) nbExit("serverConstruct: Out of memory - terminating");
      if(strncmp(str,"udp://",6)==0) str+=6;  // allow for uri
      delim=strchr(str,':');
      if(delim==NULL) len=strlen(str);
      else len=delim-str;
      if(len>15 && *str>='0' && *str<='9'){
        nbLogMsg(context,0,'E',"Inteface IP address may not be greater than 15 characters");
        nbCellDrop(context,cell);
        return(NULL);
        }
      if(len>sizeof(interfaceAddr)-1){
        nbLogMsg(context,0,'E',"Socket specification too long for buffer at--->%s",str);
        nbCellDrop(context,cell);
        return(NULL);
        }
      strncpy(interfaceAddr,str,len);
      *(interfaceAddr+len)=0; 
      if(delim!=NULL){
        delim++;
        port=(unsigned int)atoi(delim);
        }
      nbCellDrop(context,cell);
      }
    else if(type==NB_TYPE_REAL){
      r=nbCellGetReal(context,cell);
      nbCellDrop(context,cell);
      port=(unsigned int)r;
      d=port;
      if(d!=r || d==0){
        nbLogMsg(context,0,'E',"Expecting non-zero integer UDP port number");
        return(NULL);
        }
      }
    else{
      nbLogMsg(context,0,'E',"Expecting (\"file\") or (\"address[:port]\") or (port) as argument list");
      return(NULL);
      }
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'E',"The syslog skill only accepts two argument.");
      return(NULL);
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor);
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
  server->uri=uri;
  server->socket=0;
  strcpy(server->interfaceAddr,interfaceAddr);
  server->port=port;
  server->translator=translator;
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
  if((fd=nbIpGetUdpServerSocket(context,server->interfaceAddr,server->port))<0){  // 2012-12-27 eat 0.8.13 - CID 761574
    nbLogMsg(context,0,'E',"Unable to listen on port %s\n",server->port);
    return(1);
    }
  server->socket=fd;
  nbListenerAdd(context,server->socket,server,serverRead);
  if(strncmp(server->uri,"udp://",6)==0) nbLogMsg(context,0,'I',"Listening on %s for syslog",server->uri);
  else nbLogMsg(context,0,'I',"Listening on UDP port %u for syslog",server->port);
  //nbLogMsg(context,0,'I',"This is version 0.6.5");
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
    nbLogMsg(context,0,'T',"serverCommand: text=[%s]\n",text);
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

// User the server skill as default

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *syslogBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,serverConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,serverDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,serverDestroy);
  return(NULL);
  }

/*=========================================================
* Client
* 
* NOTE: The client skill is currently the same as logger, 
* but will be modified to send UDP syslog packets directly
* to a syslog receiver.  This functionality may be desired
* in some cases where export is needed even if syslogd or
* syslog-ng is down. It may also be helpful to support the
* TLS option for sending remotely to syslog-ng.  This is not
* a high priority because syslog-ng can be used for this
* purpose in most cases.
*==========================================================
*/
/*
*  The following structure is created by the skill module's "construct"
*  function (clientContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_CLIENT{      // syslog.client node descriptor
  char          *ident;            // message identifier - default "nodebrain"
  char          *uri;              // udp://filename | udp://hostname[:port]
  struct sockaddr_un un_addr;      // unix domain socket address
  int            socket;
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } NB_MOD_Client;

/*
*  construct() method
*
*    define <term> node <skill>[("<ident>"[,<uri>])][:<text>]
*
*    <ident> - name of translator file
*    <uri>   - <proto>://<spec>
*
*              <proto> - only "udp" supported currently
*              <spec>  - only local domain socket file supported currently
*
*    <text>  - flag keywords
*                trace   - display input packets
*                dump    - display dump of syslog packets
*                silent  - don't echo generated NodeBrain commands
*
*    define syslog node syslog.client("foobar");
*/
void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Client *client;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  int trace=0,dump=0,echo=1;
  char *ident="nodebrain"; 
  char *uri="udp://127.0.0.1:514";
  char *socketname=uri+6;
  int sd;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"First argument must be string message identifier");
      return(NULL);
      }
    ident=strdup(nbCellGetString(context,cell));
    if(!ident) nbExit("clientConstruct: Out of memory - terminating");
    nbCellDrop(context,cell);
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Second argument must be string uri");
      return(NULL);
      }
    uri=strdup(nbCellGetString(context,cell));
    if(!uri) nbExit("clientConstruct: Out of memory - terminating");
    nbCellDrop(context,cell);
    cell=nbListGetCellValue(context,&argSet);
    if(cell){
      nbLogMsg(context,0,'E',"The syslog.client skill only accepts two arguments.");
      return(NULL);
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor);
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
  // figure out the uri
  if(strncmp(uri,"udp://",6)){
    nbLogMsg(context,0,'E',"Expecting uri to start with 'udp://' - found %s",uri);
    return(NULL);
    }
  socketname=uri+6;
  // add code here to determine if local or remote
  sd=socket(AF_UNIX,SOCK_DGRAM,0);
  if(sd<0){
    nbLogMsg(context,0,'E',"Unable to obtain socket for %s",uri);
    return(NULL);
    }
  client=nbAlloc(sizeof(NB_MOD_Client));
  client->ident=ident;
  client->uri=uri;
  client->un_addr.sun_family=AF_UNIX;
  sprintf(client->un_addr.sun_path,"%s",socketname);
  client->socket=sd;
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
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
static int clientDisable(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *clientCommand(nbCELL context,void *skillHandle,NB_MOD_Client *client,nbCELL arglist,char *text){
  char buffer[4096];
  struct tm *tm;
  time_t utime;
  int rc;
  if(client->trace){
    nbLogMsg(context,0,'T',"clientCommand() text=[%s]\n",text);
    }
  time(&utime);
  tm=gmtime(&utime);
  sprintf(buffer,"<131>1 %4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2dZ %s",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,text); 
  rc=sendto(client->socket,buffer,strlen(buffer),MSG_DONTWAIT,(struct sockaddr *)&client->un_addr,sizeof(struct sockaddr_un));
  if(rc<0){
    nbLogMsg(context,0,'E',"Unable to send syslog message");
    }
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
static int clientDestroy(nbCELL context,void *skillHandle,NB_MOD_Client *client){
  nbLogMsg(context,0,'T',"clientDestroy called");
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

/*=========================================================
* Logger
*==========================================================
*/
/*
*  The following structure is created by the skill module's "construct"
*  function (loggerContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_LOGGER{      // syslog.logger node descriptor
  char          *ident;            // message identifier - default "nodebrain"
  unsigned char  trace;            // trace option
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } NB_MOD_Logger;

/*
*  construct() method
*
*    define <term> node <skill>[("<ident>")][:<text>]
*
*    <ident> - name of translator file
*    <text>  - flag keywords
*                trace   - display input packets
*                dump    - display dump of syslog packets
*                silent  - don't echo generated NodeBrain commands
*
*    define logger node syslog.logger("foobar");
*/
void *loggerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Logger *logger;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  int trace=0,dump=0,echo=1;
  char *ident="nodebrain";

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"First argument must be string message identifier");
      return(NULL);
      }
    ident=strdup(nbCellGetString(context,cell));
    if(!ident) nbExit("loggerConstruct: out of memory"); // 2013-01-17 eat - VID 6546
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The syslog.logger skill only accepts one argument.");
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=cursor+strlen(cursor);
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
  logger=nbAlloc(sizeof(NB_MOD_Logger));
  logger->ident=ident;
  logger->trace=trace;
  logger->dump=dump;
  logger->echo=echo;
  openlog(logger->ident,LOG_PID,LOG_LOCAL0);
  return(logger);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int loggerEnable(nbCELL context,void *skillHandle,NB_MOD_Logger *logger){
  openlog(logger->ident,LOG_PID,LOG_LOCAL0);
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
static int loggerDisable(nbCELL context,void *skillHandle,NB_MOD_Logger *logger){
  closelog();
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *loggerCommand(nbCELL context,void *skillHandle,NB_MOD_Logger *logger,nbCELL arglist,char *text){
  if(logger->trace){
    nbLogMsg(context,0,'T',"loggerCommand: text=[%s]\n",text);
    }
  syslog(LOG_INFO,"%s",text);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
static int loggerDestroy(nbCELL context,void *skillHandle,NB_MOD_Logger *logger){
  nbFree(logger,sizeof(NB_MOD_Logger));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *loggerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,loggerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,loggerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,loggerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,loggerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,loggerDestroy);
  return(NULL);
  }
