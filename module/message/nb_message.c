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
* Name:     nb_message.c
*
* Title:    Message Broadcasting Module 
*
* Function:
*
*   This program is a NodeBrain node module for broadcasting messages
*   to all nodes that are members of a group called a message "cabal".
*   It provides the following skills.
*
*     producer  - node that writes NodeBrain commands to a message log
*     consumer  - node that reads and executes NodeBrain commands from a message log
*     client    - node that reads and executes NodeBrain commands from a message server
*     server    - node that serves messages from a message log to message clients
*
*   The server skill is general purpose---not specific to the processing
*   of NodeBrain commands.  The other skills are devoted to NodeBrain
*   command processing.  However, they also serve as examples for the development
*   of other applications that need to replicate transactions for processing
*   at multiple nodes.
*
* Description:
*
*   This module is a companion to a NodeBrain messaging API (nbmsg.c).
*   The API enables any NodeBrain module to function as a message
*   producer.  The "producer" skill provided by this module is a
*   special case that executes and logs nodebrain commands.  Other
*   modules may use messages to represent transactions that are foreign
*   NodeBrain.
*
*   A producer (in terms of this module) is defined as follows.
*
*     define <term> node message.producer("<cabal>","<nodeName>"[,<nodeNumber>]);
*     # <cabal>      - name given to a set of cooperating nodes
*     # <nodeName>   - name of node within the cabal
      # <nodeNumber> - number of node within the cabal
*     <term>. define filesize cell <filesize>; # maximum file size
*
*   Messages can be initiated via the node command.
*
*     <term>:<message>
*
*   Received messages are passed to the interpreter and written to a log
*   file in the message directory.
*
*     CABOODLE/message/<cabal>/n<node>/m<time>.nbm
*
*   Messages are also received from consumers.  A producer has a
*   preferred consumer.
*
*   A consumer is defined as follows.
*
*     define <term> node message.consumer("<cabal>",<node>);
*     # <cabal>    - name given to a set of cooperating nodes
*     # <node>     - number of this node within the cabal
*     <term>. define retry cell <retry>; # seconds between connection retries
*
*   Messages are read from a log by a consumer and sent to a prefered
*   peer within the cabal. 
*
*===========================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2009-12-13 Ed Trettevik - original prototype version 0.7.7
* 2009-12-30 eat 0.7.7  Organized into producer/consumer/server/client/peer skills
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-04-26 eat 0.7.9  Included optional node number argument to message.producer
* 2011-02-08 eat 0.8.5  Started to unify message.(client|server|peer)
* 2012-04-22 eat 0.8.8  Included message.prune command
* 2012-10-17 eat 0.8.12 Checker updates
* 2012-10-18 eat 0.8.12 Checker updates
* 2012-12-25 eat 0.8.13 AST 39
* 2012-12-27 eat 0.8.13 Checker updates
*===================================================================================
*/
#include "config.h"
#include <nb/nb.h>
#include <nb/nbtls.h>

/*==================================================================================
*  Producer - just uses the Message Log API (not Message Peer API)
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function (messageContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_PRODUCER{    // message.producer node descriptor
  char           cabalName[32];    // cabal name identifying a group of nodes
  char           nodeName[32];     // name of node within cabal
  int            cabalNode;        // number of node within cabal
  nbMsgLog      *msglog;           // msglog structure
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModProducer;

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
*    define <term> node message.producer("<cabal>","<node>",<number>);
*/
void *producerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModProducer *producer;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int cabalNode=0;
  double cabalNodeReal;
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying node");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument must not exceed %d characters",sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  // optional node number
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    type=nbCellGetType(context,cell);
    if(type!=NB_TYPE_REAL){
      nbLogMsg(context,0,'E',"Third argument must be number identifying node");
      return(NULL);
      }
    cabalNodeReal=nbCellGetReal(context,cell);
    cabalNode=(int)cabalNodeReal;
    if((float)cabalNode!=cabalNodeReal || cabalNode<0 || cabalNode>255){
      nbLogMsg(context,0,'E',"Third argument must be integer node number from 0 to 255");
      return(NULL);
      }
    nbCellDrop(context,cell);
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'E',"The message.producer skill only accepts three arguments.");
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
    while(*cursor==' ') cursor++;
    }
  producer=nbAlloc(sizeof(nbModProducer));
  memset(producer,0,sizeof(nbModProducer));
  strcpy(producer->cabalName,cabalName);
  strcpy(producer->nodeName,nodeName);
  producer->cabalNode=cabalNode;
  producer->msglog=NULL;
  producer->trace=trace;
  producer->dump=dump;
  producer->echo=echo;
  if(producer->trace) nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(producer);
  }

/*
*  enable() method
*
*    enable <node>
*/
int producerEnable(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbMsgLog *msglog;
  nbMsgState *msgstate;
  int state;

  // We set the message state to NULL to let the message log define our state
  // for other applications we may want to initialize a message state and
  // reprocess messages from the log to resynchronize
  //msgstate=nbMsgStateCreate(context);
  msgstate=NULL;
  //time(&utime);
  //msgTime=utime;
  //msgCount=0;
  //nbMsgStateSet(msgstate,producer->cabalNode,msgTime,msgCount);
  msglog=nbMsgLogOpen(context,producer->cabalName,producer->nodeName,producer->cabalNode,"",NB_MSG_MODE_PRODUCER,msgstate);
  if(!msglog){
    nbLogMsg(context,0,'E',"Unable to open message log for cabal \"%s\" node %d",producer->cabalName,producer->cabalNode);
    return(1);
    }
  producer->msglog=msglog;
  while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND));
  //nbLogMsg(context,0,'T',"State after read loop is %d",state);
  if(state<0){
    nbLogMsg(context,0,'E',"Unable to read to end of file for cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(1);
    }
  //state=nbMsgLogProduce(context,msglog,1024*1024); // smaller files to force more file boundaries for testing messaging
  state=nbMsgLogProduce(context,msglog,10*1024*1024);
  //nbLogMsg(context,0,'T',"Return from nbMsgLogProduce() is %d",state);
  nbLogMsg(context,0,'I',"Enabled for cabal %s node %s",msglog->cabal,msglog->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int producerDisable(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbMsgLogClose(context,producer->msglog);
  producer->msglog=NULL;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *producerCommand(nbCELL context,void *skillHandle,nbModProducer *producer,nbCELL arglist,char *text){
  if(producer->trace){
    nbLogMsg(context,0,'T',"nb_message:producerCommand() text=[%s]\n",text);
    }
  if(producer->msglog) nbMsgLogWriteString(context,producer->msglog,(unsigned char *)text);
  else nbLogMsg(context,0,'E',"nb_message:producerCommand():message log not open");
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int producerDestroy(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbLogMsg(context,0,'T',"producerDestroy called");
  if(producer->msglog) producerDisable(context,skillHandle,producer);
  nbFree(producer,sizeof(nbModProducer));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *producerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,producerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,producerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,producerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,producerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,producerDestroy);
  return(NULL);
  }


/*==================================================================================
*  Consumer
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function (messageContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_CONSUMER{  // message.consumer node descriptor
  char           cabalName[32];    // cabal name identifies a group of nodes
  char           nodeName[32];     // name of node within cabal
  int            cabalNode;        // number of node within cabal
  char           name[32];         // consumer name
  nbMsgState    *msgstate;         // consumer message state
  nbMsgLog      *msglog;           // msglog structure
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModConsumer;

int consumerMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModConsumer *consumer=handle;
  char *cmd;
  int cmdlen;

  if(consumer->trace) nbLogMsg(context,0,'T',"consumerMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

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
*    define <term> node message.consumer("<cabal>","<nodeName>",<nodeNumber>,"<consumerName>");
*/
void *consumerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModConsumer *consumer;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  char consumerName[32];
  int cabalNode=0;
  double cabalNodeReal;
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must not exceed %d characters",sizeof(cabalName)-1);
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);

  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying a node within the cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument must not exceed %d characters",sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);

  // node number
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node number required as third argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_REAL){
    nbLogMsg(context,0,'E',"Third argument must be number identifying node");
    return(NULL);
    }
  cabalNodeReal=nbCellGetReal(context,cell);
  cabalNode=(int)cabalNodeReal;
  if((float)cabalNode!=cabalNodeReal || cabalNode<0 || cabalNode>255){
    nbLogMsg(context,0,'E',"Third argument must be integer node number from 0 to 255");
    return(NULL);
    }
  nbCellDrop(context,cell);

  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Consumer name required as forth argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Fourth argument must be string identifying a consumer");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(consumerName)-1){
    nbLogMsg(context,0,'E',"Fourth argument must not exceed %d characters",sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(consumerName,str);  // size checked
  nbCellDrop(context,cell);

  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.consumer skill only accepts two arguments.");
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
    while(*cursor==' ') cursor++;
    }
  consumer=nbAlloc(sizeof(nbModConsumer));
  memset(consumer,0,sizeof(nbModConsumer));
  strcpy(consumer->cabalName,cabalName);
  strcpy(consumer->nodeName,nodeName);
  consumer->cabalNode=cabalNode;
  strcpy(consumer->name,consumerName);
  consumer->cabalNode=cabalNode;
  consumer->trace=trace;
  consumer->dump=dump;
  consumer->echo=echo;
  if(consumer->trace) nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(consumer);
  }

/*
*  enable() method
*
*    enable <node>
*/
int consumerEnable(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  //consumer->msgstate=nbMsgStateCreate(context);
  //consumer->msglog=nbMsgLogOpen(context,consumer->cabalName,consumer->nodeName,consumer->cabalNode,"",NB_MSG_MODE_CONSUMER,consumer->msgstate);
  consumer->msglog=nbMsgLogOpen(context,consumer->cabalName,consumer->nodeName,consumer->cabalNode,consumer->name,NB_MSG_MODE_CURSOR,NULL);
  if(!consumer->msglog){
    nbLogMsg(context,0,'E',"consumerEnable: Unable to open message log for cabal \"%s\" node %d",consumer->cabalName,consumer->cabalNode);
    return(1);
    }
  if(nbMsgLogConsume(context,consumer->msglog,consumer,consumerMessageHandler)!=0){
    nbLogMsg(context,0,'E',"Unable to register message handler for cabal \"%s\" node %d",consumer->cabalName,consumer->cabalNode);
    return(1);
    }
  nbLogMsg(context,0,'I',"Enabled for cabal %s node %s",consumer->cabalName,consumer->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int consumerDisable(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  //nbMsgCacheClose(context,consumer->msgcache);
  // take care of TLS structures here
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *consumerCommand(nbCELL context,void *skillHandle,nbModConsumer *consumer,nbCELL arglist,char *text){
  if(consumer->trace){
    nbLogMsg(context,0,'T',"nb_message:consumerCommand() text=[%s]\n",text);
    }
  nbCmd(context,text,1);
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int consumerDestroy(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  nbLogMsg(context,0,'T',"consumerDestroy called");
  consumerDisable(context,skillHandle,consumer);
  nbFree(consumer,sizeof(nbModConsumer));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *consumerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,consumerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,consumerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,consumerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,consumerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,consumerDestroy);
  return(NULL);
  }

/*==================================================================================
*  Peer
*
*  There are currently three skills that share a common peer structure.
*
*    client - processes messages from a server
*    server - shares messages with a client
*    peer   - both client and server
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function defined in this file.  This is a module specific structure.
*  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/

typedef struct NB_MOD_PEER{        // message.client node descriptor
  char           cabalName[32];    // cabal name
  char           nodeName[32];     // node name within cabal
  int            cabalNode;        // number of node within cabal
  nbMsgCabal     *msgpeer;         // client mode peer
  unsigned char  trace;            // trace option
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModPeer;

/*
*  Peer message handler
*/
int peerMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModPeer *peer=handle;
  char *cmd;
  int cmdlen;

  if(peer->trace) nbLogMsg(context,0,'T',"clientMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

/*
*  Client message handler
*/
int clientMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModPeer *client=handle;
  char *cmd;
  int cmdlen;

  if(client->trace) nbLogMsg(context,0,'T',"clientMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

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
*    define <term> node message.peer("<cabal>","<nodeName>");
*    <term>. define filelines cell <filelines>; # number of lines per file
*/
void *peerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModPeer *peer;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying node within cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument has node name \"%s\" too long for buffer - limit is %d characters",str,sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.(peer|client|server) skill only accepts two arguments.");
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
    while(*cursor==' ') cursor++;
    }
  peer=nbAlloc(sizeof(nbModPeer));
  memset(peer,0,sizeof(nbModPeer));
  strcpy(peer->cabalName,cabalName);
  strcpy(peer->nodeName,nodeName);
  peer->msgpeer=NULL;
  peer->trace=trace;
  peer->dump=dump;
  peer->echo=echo;
  if(peer->trace) nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(peer);
  }

/*
*  enable() method
*
*    enable <node>
*/
int peerEnable(nbCELL context,void *skillHandle,nbModPeer *server){
  if(!server->msgpeer) server->msgpeer=nbMsgCabalOpen(context,NB_MSG_CABAL_MODE_PEER,server->cabalName,server->nodeName,NULL,NULL,peerMessageHandler);
  if(!server->msgpeer){
    nbLogMsg(context,0,'E',"Unable to instantiate message peer for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,server->msgpeer);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int peerDisable(nbCELL context,void *skillHandle,nbModPeer *peer){
  nbMsgCabalDisable(context,peer->msgpeer);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *peerCommand(nbCELL context,void *skillHandle,nbModPeer *peer,nbCELL arglist,char *text){
  if(peer->trace){
    nbLogMsg(context,0,'T',"nb_message:peerCommand() text=[%s]\n",text);
    }
  //nbCmd(context,text,1);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
int peerDestroy(nbCELL context,void *skillHandle,nbModPeer *peer){
  nbLogMsg(context,0,'T',"peerDestroy called");
  if(peer->msgpeer) peerDisable(context,skillHandle,peer);
  nbFree(peer,sizeof(nbModPeer));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *peerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,peerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,peerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,peerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,peerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,peerDestroy);
  return(NULL);
  }

/*==================================================================================
*  Client
*=================================================================================*/

/*
*  enable() method
*
*    enable <node>
*/
int clientEnable(nbCELL context,void *skillHandle,nbModPeer *client){
  // We call nbMsgCabalClient with a NULL message state to let the message log
  // define our state.  For other applications we may want to set the message state
  // from another source, and reprocess message from the log to resynchronize.
  if(!client->msgpeer) client->msgpeer=nbMsgCabalClient(context,client->cabalName,client->nodeName,client,clientMessageHandler);
  if(!client->msgpeer){
    nbLogMsg(context,0,'E',"Unable to instantiate message client for cabal \"%s\" node \"%s\"",client->cabalName,client->nodeName);
    return(1);
    }
  if(nbMsgCabalClientSync(context,client->msgpeer,NULL)){
    nbLogMsg(context,0,'E',"Unable to synchronize message client for cabal \"%s\" node \"%s\"",client->cabalName,client->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,client->msgpeer);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",client->cabalName,client->nodeName);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int clientCommand(nbCELL context,void *skillHandle,nbModPeer *client,nbCELL arglist,char *text){
  if(!client || !client->msgpeer || !client->msgpeer->msglog){
    nbLogMsg(context,0,'T',"nb_message: clientCommand() text: %s",text);
    nbLogMsg(context,0,'T',"nb_message: client is not properly enabled - check prior messages");
    return(1);
    }
  if(client->trace){
    nbLogMsg(context,0,'T',"nb_message: clientCommand() text: %s",text);
    }
  nbCmd(context,text,1);
  if(client->msgpeer->msglog) nbMsgLogWriteString(context,client->msgpeer->msglog,(unsigned char *)text);
  else nbLogMsg(context,0,'E',"nb_message:clientCommand():message log file not open");
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *clientBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,peerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,peerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,peerDestroy);
  return(NULL);
  }

/*==================================================================================
*  Server
*=================================================================================*/

/*
*  enable() method
*
*    enable <node>
*/
int serverEnable(nbCELL context,void *skillHandle,nbModPeer *server){
  if(!server->msgpeer) server->msgpeer=nbMsgCabalServer(context,server->cabalName,server->nodeName);
  if(!server->msgpeer){
    nbLogMsg(context,0,'E',"Unable to instantiate message peer server for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,server->msgpeer);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *serverCommand(nbCELL context,void *skillHandle,nbModPeer *server,nbCELL arglist,char *text){
  if(server->trace){
    nbLogMsg(context,0,'T',"nb_message:serverCommand() text=[%s]\n",text);
    }
  //nbCmd(context,text,1);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *serverBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,peerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,peerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,peerDestroy);
  return(NULL);
  }

//=====================================================================
// Commands

int messageCmdParseLogIdentifiers(nbCELL context,char **cursorP,char *cabalName,char *nodeName,int *instance){
  char *cursor=*cursorP;
  char instanceStr[4];
  char *delim;
  int len;

  // consider using nbParseToken instead
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>63){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character cabal name at:%s",63,cursor);
    return(1);
    }
  strncpy(cabalName,cursor,len);
  *(cabalName+len)=0;
  cursor+=len;
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>63){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character node name at:%s",63,cursor);
    return(1);
    }
  strncpy(nodeName,cursor,len);
  *(nodeName+len)=0;
  cursor+=len;
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>sizeof(instanceStr)-1){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character instance number at:%s",sizeof(instanceStr)-1,cursor);
    return(1);
    }
  strncpy(instanceStr,cursor,len);
  *(instanceStr+len)=0;
  cursor+=len;
  *cursorP=cursor;
  *instance=atoi(instanceStr); 
  return(0);
  }

int messageCmdInitialize(nbCELL context,void *handle,char *verb,char *cursor){
  char cabalName[64],nodeName[64];
  int instance;
  char *delim;
  int len;
  int option;

  if(strcmp(verb,"message.create")==0) option=NB_MSG_INIT_OPTION_CREATE;
  else if(strcmp(verb,"message.convert")==0) option=NB_MSG_INIT_OPTION_CONVERT;
  else if(strcmp(verb,"message.empty")==0) option=NB_MSG_INIT_OPTION_EMPTY;
  else{
    nbLogMsg(context,0,'E',"Message verb %s not recognized.",verb);
    return(1);
    }
  if(option!=NB_MSG_INIT_OPTION_CREATE){
    nbLogMsg(context,0,'E',"Message verb %s not implemented.",verb);
    return(1);
    }
  if(messageCmdParseLogIdentifiers(context,&cursor,cabalName,nodeName,&instance)) return(1);
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ' && *delim!=';') delim++;
  len=delim-cursor;
  if(len==0 || (len==7 && strncmp(cursor,"content",7)==0)) option|=NB_MSG_INIT_OPTION_CONTENT;
  else if(len==5 && strncmp(cursor,"state",5)==0) option|=NB_MSG_INIT_OPTION_STATE;
  else{
    nbLogMsg(context,0,'E',"Expecting type of 'content' or 'state' at:%s",cursor);
    return(1);
    }
  while(*delim==' ') delim++;
  if(*delim && *delim!=';'){
    nbLogMsg(context,0,'E',"Unexpecting text at:%s",delim);
    return(1);
    }
  if(nbMsgLogInitialize(context,cabalName,nodeName,instance,option)) return(1);
  return(0);
  }

/*
*  Retire message files older than a specified time period
*
*     message.prune <cabal> <node> <instance> <n><timeunit>
*
*     <n>        - number of time units to retain before retiring
*     <timeunit> - time unit (d)ay, (h)our, (m)inute, (s) second
*
*  At least one file will always be retained, and may be open for
*  writing by the owning process. 
*/
int messageCmdRetire(nbCELL context,void *handle,char *verb,char *cursor){
  char cabalName[64],nodeName[64];
  int instance;
  char *delim;
  int number;
  unsigned int seconds,twentyfour=24,sixty=60;

  if(messageCmdParseLogIdentifiers(context,&cursor,cabalName,nodeName,&instance)) return(1);
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ' && *delim!=';') delim++;
  number=atoi(cursor); // get number
  if(number<1 || number>20000){
    nbLogMsg(context,0,'E',"Expecting number from 1 to 20,000 at:%s",cursor);
    return(1);
    }
  seconds=number;
  while(*cursor>='0' && *cursor<='9') cursor++; // step over n
  switch(*cursor){
    case 'd': seconds*=twentyfour*sixty*sixty; break;  // 2012-12-27 eat - CID 751560 - included compile time multiplication and break
    case 'h': seconds*=sixty*sixty; break;             // 2012-12-27 eat - CID 751561 - included compile time multiplication and break
    case 'm': seconds*=sixty; break;
    case 's': break;
    default:
      nbLogMsg(context,0,'E',"Expecting time unit of 'd', 'h', 'm', or 's' at:%s",cursor);
      return(1);
    }
  cursor++;
  while(*cursor==' ') cursor++;
  if(*cursor && *cursor!=';'){
    nbLogMsg(context,0,'E',"Unexpecting text at:%s",cursor);
    return(1);
    }
  if(nbMsgLogPrune(context,cabalName,nodeName,instance,seconds)) return(1);
  return(0);
  }


/*
*  Export a message file converting to text.
*/
int messageCmdExport(nbCELL context,void *handle,char *verb,char *cursor){
  char cabalName[64],nodeName[64],instanceStr[4],filename[64];
  int instance;
  char *delim;
  int len;
  nbMsgLog *msglog;
  int state;

  // After testing the messageCmdCreate command call nbMsgCmdParseLogIdentifiers here also
  // consider using nbParseToken instead
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character cabal name at:%s",sizeof(cabalName)-1,cursor);
    return(1);
    } 
  strncpy(cabalName,cursor,len);
  *(cabalName+len)=0;
  cursor+=len;
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character node name at:%s",sizeof(nodeName)-1,cursor);
    return(1);
    }
  strncpy(nodeName,cursor,len);
  *(nodeName+len)=0;
  cursor+=len;
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>sizeof(instanceStr)-1){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character instance number at:%s",sizeof(instanceStr)-1,cursor);
    return(1);
    }
  strncpy(instanceStr,cursor,len);
  *(instanceStr+len)=0;
  cursor+=len;
  while(*cursor==' ') cursor++;
  delim=cursor;
  while(*delim && *delim!=' ') delim++;
  len=delim-cursor;
  if(!len || len>sizeof(filename)-1){
    nbLogMsg(context,0,'E',"Expecting 1 to %d character file name at:%s",sizeof(filename)-1,cursor);
    return(1);
    }
  strncpy(filename,cursor,len);
  *(filename+len)=0;
  cursor+=len;
 
  instance=atoi(instanceStr); 
  msglog=nbMsgLogOpen(context,cabalName,nodeName,instance,filename,NB_MSG_MODE_SINGLE,NULL); 
  if(!msglog){
    nbLogMsg(context,0,'E',"Unable to open message log for cabal \"%s\" instance %d",cabalName,instance);
    return(1);
    }
  nbMsgPrint(stdout,msglog->msgrec);
  while(!((state=nbMsgLogRead(context,msglog))&(NB_MSG_STATE_LOGEND|NB_MSG_STATE_FILEND))){
    nbMsgPrint(stdout,msglog->msgrec);
    }
  if(state&NB_MSG_STATE_FILEND) nbMsgPrint(stdout,msglog->msgrec);
  if(state<0){
    nbLogMsg(context,0,'E',"Unable to read to end of file for cabal \"%s\" node %d",cabalName,instance);
    return(1);
    }
  nbLogMsg(context,0,'I',"Cabal '%s' node '%s' instance %u exported",cabalName,nodeName,instance);
  return(0);
  }

//=====================================================================
/* Module Initialization Function */

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *nbBind(nbCELL context,char *ident,nbCELL arglist,char *text){
  nbVerbDeclare(context,"message.create",NB_AUTH_CONTROL,0,NULL,&messageCmdInitialize,"<cabal> <node> <instance> [content|state]");
  nbVerbDeclare(context,"message.convert",NB_AUTH_CONTROL,0,NULL,&messageCmdInitialize,"<cabal> <node> <instance> [content|state]");
  nbVerbDeclare(context,"message.empty",NB_AUTH_CONTROL,0,NULL,&messageCmdInitialize,"<cabal> <node> <instance> [content|state]");
  nbVerbDeclare(context,"message.prune",NB_AUTH_CONTROL,0,NULL,&messageCmdRetire,"<cabal> <node> <instance> [<n><period>]");
  nbVerbDeclare(context,"message.export",NB_AUTH_CONTROL,0,NULL,&messageCmdExport,"<cabal> <node> <instance> <file>");
  return(NULL);
  }

