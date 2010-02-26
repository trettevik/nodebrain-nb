/*
* Copyright (C) 2005-2010 The Boeing Company
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
* Name:   nb_syslog.c
*
* Title:  Remote Syslog Monitor 
*
* Function:
*
*   This program is a NodeBrain skill module for monitoring syslog
*   data exported from remote servers.
*   
* Reference:    RFC 3164 - "The BSD syslog Protocol"
*
* Description:
*
*   This is an experimental syslog monitor.  Unless you have a special
*   reason to accept syslog data directly for monitoring, you may prefer
*   to use a standard syslogd to accept the UDP packets and use NodeBrain
*   to monitor the syslog file.
*
*   This module listens on UDP port 514, although an alternate port may be
*   used for experimentation. 
*
*   You may define one or more syslog nodes, but only one can listen
*   to a give port on a given interface.
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
*       define syslog node syslog("syslog.nbx"); 
*       define syslog node syslog("syslog.nbx","127.0.0.1"); # bind to local host 
*       define syslog node syslog("syslog.nbx",50514);  # alternate port
*       define syslog node syslog("syslog.nbx","127.0.0.1:50514");  # both
*       define syslog node syslog("syslog.nbx"):dump;  # specify an option
*       define syslog node syslog("syslog.nbx",50514):silent; # specify port and option
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
*       syslogConstruct()  - construct an syslog node
*       syslogEnable()     - start listening
*       syslogDisable()    - stop listening
*       syslogCommand()    - handle a custom command
*       syslogDestroy()    - clean up when the node is destroyed
*
*   Interface to NodeBrain objects (primarily used in syslogConstruct)
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
*       syslogRead()      - Handler passed to nbListenerAdd()   
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
*=====================================================================
*/
#include "config.h"
#include <nb.h>

/*
*  The following structure is created by the skill module's "construct"
*  function (syslogContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
struct NB_MOD_SYSLOG{              /* SYSLOG node descriptor */
  unsigned int   socket;           /* server socket for datagrams */
  char interfaceAddr[16];              /* interface address to bind listener */
  unsigned short port;             /* UDP port of listener */
  struct NB_CELL *translator;             /* syslog message text translator */
  unsigned char  trace;            /* trace option */
  unsigned char  dump;             /* option to dump packets in trace */
  unsigned char  echo;             /* echo option */
  unsigned int   sourceAddr;       /* source address */
  };

typedef struct NB_MOD_SYSLOG NB_MOD_Syslog;

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
*  vary in the set of methods they implement.  For example, syslogRead() is an
*  example of a method that would only be used by a module that "listens".
*
*=================================================================================*/

/*
*  Read incoming packets
*/
void syslogRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Syslog *syslog=handle;
  unsigned char buffer[NB_BUFSIZE];
  size_t buflen=NB_BUFSIZE;
  int  len;
  unsigned short rport;
  unsigned char daddr[40],raddr[40];

  nbIpGetSocketAddrString(serverSocket,daddr);
  len=nbIpGetDatagram(context,serverSocket,&syslog->sourceAddr,&rport,buffer,buflen);
	if(syslog->trace) nbLogMsg(context,0,'I',"Datagram %s:%5.5u -> %s len=%d",nbIpGetAddrString(raddr,syslog->sourceAddr),rport,daddr,len);
  if(syslog->dump) nbLogDump(context,buffer,len);
  *(buffer+len)=0;  // make sure we have a null terminator
  nbTranslatorExecute(context,syslog->translator,buffer);
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
*    define syslog node syslog("syslog.nbx");
*    define syslog node syslog("syslog.nbx"):dump,silent;
*    define syslog node syslog("syslog.nbx","127.0.0.1");
*    define syslog node syslog("syslog.nbx",50162);
*    define syslog node syslog("syslog.nbx","127.0.0.1:50162");
*    define syslog node syslog("syslog.nbx","127.0.0.1:50162"):silent;
*/
void *syslogConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Syslog *syslog;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  double r,d;
  char interfaceAddr[16];
  unsigned int port=514;
  int type,trace=0,dump=0,echo=1;
  int len;
  char *str;
  char *transfilename;
  nbCELL translator;

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
  translator=nbTranslatorCompile(context,transfilename);
  if(translator==NULL){
    nbLogMsg(context,0,'E',"Unable to load translator '%s'",transfilename);
    return(NULL);
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type==NB_TYPE_STRING){
      str=nbCellGetString(context,cell);
      delim=strchr(str,':');
      if(delim==NULL) len=strlen(str);
      else len=delim-str;
      if(len>15){
        nbLogMsg(context,0,'E',"Inteface IP address may not be greater than 15 characters");
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
      nbLogMsg(context,0,'E',"Expecting interface (\"address[:port]\") or (port) as argument list");
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
    if(delim!=NULL){
      saveDelim=*delim;
      *delim=0;
      }
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    if(delim!=NULL){
      *delim=saveDelim;
      cursor=delim;
      while(*cursor==' ' || *cursor==',') cursor++;
      }
    else cursor=strchr(cursor,0);
    }
  syslog=malloc(sizeof(NB_MOD_Syslog));
  syslog->socket=0;
  strcpy(syslog->interfaceAddr,interfaceAddr);
  syslog->port=port;
  syslog->translator=translator;
  syslog->trace=trace;
  syslog->dump=dump;
  syslog->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(syslog);
  }

/*
*  enable() method
*
*    enable <node>
*/
int syslogEnable(nbCELL context,void *skillHandle,NB_MOD_Syslog *syslog){
  if((syslog->socket=nbIpGetUdpServerSocket(context,syslog->interfaceAddr,syslog->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on port %s\n",syslog->port);
    return(1);
    }
  nbListenerAdd(context,syslog->socket,syslog,syslogRead);
  nbLogMsg(context,0,'I',"Listening on UDP port %u for syslog",syslog->port);
  //nbLogMsg(context,0,'I',"This is version 0.6.5");
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int syslogDisable(nbCELL context,void *skillHandle,NB_MOD_Syslog *syslog){
  nbListenerRemove(context,syslog->socket);
#if defined(WIN32)
  closesocket(syslog->socket);
#else
  close(syslog->socket);
#endif
  syslog->socket=0;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *syslogCommand(nbCELL context,void *skillHandle,NB_MOD_Syslog *syslog,nbCELL arglist,char *text){
  if(syslog->trace){
    nbLogMsg(context,0,'T',"nb_syslog:syslogCommand() text=[%s]\n",text);
    }
  /* insert command parsing code here */
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int syslogDestroy(nbCELL context,void *skillHandle,NB_MOD_Syslog *syslog){
  nbLogMsg(context,0,'T',"syslogDestroy called");
  if(syslog->socket!=0) syslogDisable(context,skillHandle,syslog);
  free(syslog);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *syslogBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,syslogConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,syslogDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,syslogEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,syslogCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,syslogDestroy);
  return(NULL);
  }
