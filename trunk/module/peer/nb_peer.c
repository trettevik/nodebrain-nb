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
*
* Name:   nb_peer.c
*
* Title:  NodeBrain Peer Communications Skill Module 
*
* Function:
*
*   This program is a NodeBrain skill module that supports peer-to-peer
*   communication between two skulls (NodeBrain processes).
*   
* Synopsis:
*
*   define <term> node peer.server("<socketSpec>");
*   define <term> node peer("<socketSpec>[{interval}][(queueSpec)]"[,<schedule>]);
*   define <term> node peer.queue("<queueSpec>"[,<schedule>]);
*
*     <socketSpec>  - Peer specification
*                     [<myIdentity>~]<identity>@<host>:<port>
*                     [<myIdentity>~]<identity>@<socketfile>
*     <identity>    - Identity name (must be declared)
*     <host>        - Name or IP address
*     <port>        - Port number
*
*     <interval>    - Seconds to hold connection without use
*
*     <queueSpec>   - Queue specification
*                     <queueDir>[(queueTime)]
*     <queueDir>    - Queue directory. Files are stored in sub-directories
*                     by identity.
*                     <queueDir>/<identity>/<file>
*     <queueTime>   - Seconds to write to the same file 
*
* Description:
*
*   This modules supports the NodeBrain Protocol (NBP) and NodeBrain queues (NBQ).
*
* History:
*
*   Peer server skill replaces old NBP listener.
*
*     portray <identity>;
*     define <term> listener type="NBP",interface="<host>",port=<port>;
*   
*       -now-
*
*     define <term> node peer.server("<identity>@<host>:port");
*
*   Peer and queue skills replaces old BRAIN prefix notation.
*
*     declare <term> brain <identity>@<host>:<port>[{interval}][(queueSpec)];
*     ><term> <command>
*     \<term> <command>
*     /<term> <command>
*       
*       -now-
*
*     # with peer
*     define <term> node peer("...without queue...");
*     <term>:<command>                       [ Replaces ><term> <command> ]
*     define <term> node peer("...with queue and no schedule...");
*     <term>:<command>                       [ Replaces /<term> <command> ]
*     define <term> node peer("...with queue and schedule...");
*     <term>:<command>                       [ Replaces \<term> <command> ]
*     # without peer
*     define <term> node peer("...with queue and no schedule...");
*     <term>:<command>                       [ Replaces \<term> <command> ]
*
*     # Special commands support override to default behavior
*     define <term> node peer("...with queue, with or without schedule...");
*     <term>(0):<command>                    [ Replaces \<term> <command> ]
*     <term>(1):<command>                    [ Replaces /<term> <command> ]
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2007/06/23 Ed Trettevik - original skill module prototype version
* 2007/06/23 eat 0.6.8  Structured node module around old NBP listener code
* 2008/03/07 eat 0.6.9  Moved parsing for self identity to nbBrainNew()
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2012-01-31 dtl 0.8.7  Checker updates.
* 2012-02-09 eat 0.8.7  Reviewed Checker
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-10-13 eat 0.8.12 Replaced exit with nbExit
* 2012-10-17 eat 0.8.12 Added size parameter to nbNodeGetNameFull
* 2012-12-16 eat 0.8.13 Checker updates.
* 2012-12-27 eat 0.8.13 Checker updates.
*=====================================================================
*/
//#include "config.h"
#include <nb/nbi.h>    // WARNING: We should use nb.h instead - after removing dependence on internal functions

#include "nbbrain.h"
#include "nbske.h"
#include "nbvli.h"
#include "nbpke.h"
#include "nbchannel.h"
#include "nbprotokey.h"
#include "nbprotocol.h"
#include <nb/nbqueue.h>

#include "nb_peer.h"  // 2009-04-19 eat 0.7.5 - used to expose client to nbprotocol for queue transfer

static unsigned short nb_mode_embraceable=0;  /* flag server as deadly embraceable---has NBP listener */
//=============================================================================
 
typedef struct NB_MOD_NBP_SERVER{
  NB_PeerKey      *identity;     // identity
  char             idName[64];   // identity name
  char             hostname[512]; // hostname to bind to
  char             address[512]; // address to bind
  unsigned short   port;         // port to listen on
  int              socket;       // socket we are listening on
  char             oar[512];     // oar rule file
  struct LISTENER  *ear;         // old listener to support transition 
  } nbServer;

//==================================================================================
// Create new server structure from server specification
//
//    identity@address:port

static nbServer *newServer(nbCELL context,char *cursor,char *oar,char *msg){
  nbServer *server;
  char *inCursor;
  char *interfaceAddr;

  if(strlen(oar)>=sizeof(server->oar)){  // 2012-12-16 eat - 751650
    sprintf(msg,"Oar is too long for buffer - %s",oar);
    return(NULL);
    }
  server=nbAlloc(sizeof(nbServer));
  inCursor=server->idName;
  while(*cursor==' ') cursor++;
  while(*cursor && *cursor!='@'){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*cursor!='@'){
    sprintf(msg,"Identity not found in server specification - expecting identity@address:port or identity@file");
    return(NULL);
    }
  cursor++;
  server->identity=nbpGetPeerKey(server->idName);
  if(server->identity==NULL){
    snprintf(msg,(size_t)NB_MSGSIZE,"Identity '%s' not defined",server->idName); //2012-01-31 dtl: replaced sprintf
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
  if(strchr(server->address,'/')) server->port=0;  // local domain socket
  else{
    if(*server->address<'0' || *server->address>'9'){
      strcpy(server->hostname,server->address);
      interfaceAddr=chgetaddr(server->address);
      if(interfaceAddr==NULL){
        snprintf(msg,(size_t)NB_MSGSIZE,"Hostname %s not resolved",server->hostname); //2012-01-31 dtl: replaced sprintf
        nbFree(server,sizeof(nbServer));
        return(NULL);
        }
      strncpy(server->address,interfaceAddr,sizeof(server->address)-1);
      *(server->address+sizeof(server->address)-1)=0;
      } 
    else{
      interfaceAddr=chgetname(server->address);
      if(interfaceAddr==NULL) *server->hostname=0;
      else{
        strncpy(server->hostname,interfaceAddr,sizeof(server->hostname));
        *(server->hostname+sizeof(server->hostname)-1)=0;
        }
      }
    if(*cursor!=':'){
      snprintf(msg,(size_t)NB_MSGSIZE,"Expecting ':port' at: %s",cursor); //2012-01-31 dtl: replaced sprintf
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
    }
  server->socket=0;
  strcpy(server->oar,oar);
  server->ear=NULL;
  return(server); 
  }

//==================================================================================
//
// Handle connection requests
//
static void serverAccept(nbCELL context,int serverSocket,void *handle){
  nbServer *server=handle;
  struct NBP_SESSION *session;

  if((session=nbpNewSessionHandle(server->identity))==NULL){
    nbLogMsg(context,0,'L',"Unable to obtain a session handle.");
    return;
    }
  if(chaccept(session->channel,(int)server->socket)<0){
    if(errno!=EINTR) nbLogMsg(context,0,'E',"serverAccept: chaccept failed errno=%d",errno);
    nbpFreeSessionHandle(session);   // 2012-12-18 eat - CID 751613
    }
  else{
    //2008-06-11 eat - the next line is a goofy attept to provide a Unix domain socket name for nbpServe to print
    if(server->port==0) snprintf(session->channel->unaddr,256,"%s",server->address); //2012-01-31 dtl: replaced strcpy
    nbpServe(server->ear,session,1,server->oar);
    }
  }

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*
*    define <term> node peer.server("<serverSpec>"[,<oar>]);
*/
static void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbServer *server;
  nbCELL cell=NULL,specCell=NULL;
  nbSET argSet;
  char *serverSpec,*oar="";
  char msg[1024];

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string server specification as first parameter - identity@address:port");
    return(NULL);
    }
  serverSpec=nbCellGetString(context,cell);
  specCell=cell;
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Expecting string oar rule file name as second parameter");
      return(NULL);
      }
    oar=nbCellGetString(context,cell);
    if(nbListGetCellValue(context,&argSet)!=NULL){
      nbLogMsg(context,0,'E',"Peer server only accepts two arguments");
      return(NULL);
      }
    }
  server=newServer(context,serverSpec,oar,msg);
  if(server==NULL){
    nbLogMsg(context,0,'E',msg);
    return(NULL);
    }
  nbCellDrop(context,specCell);
  if(cell) nbCellDrop(context,cell);
  // create old listener to support transition
  server->ear=nbpListenerNew(context);
  server->ear->port=server->port;
  server->ear->address=(NB_String *)nbCellCreateString(context,server->address);
  server->ear->identity=server->identity;
  
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(server);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int serverEnable(nbCELL context,void *skillHandle,nbServer *server){
  // we should use an API function here
  if((server->socket=nbIpListen(server->address,server->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on %s:%u",server->address,server->port);
    return(1);
    }
  server->ear->fildes=server->socket;  // for old code
  nb_mode_embraceable=1;                       // flag this skull as embraceable
  nbListenerAdd(context,server->socket,server,serverAccept);
  if(server->port==0) nbLogMsg(context,0,'I',"Listening for NBP connections as %s@%s",server->idName,server->address);
  else nbLogMsg(context,0,'I',"Listening for NBP connections as %s@%s:%u",server->idName,server->address,server->port);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
static int serverDisable(nbCELL context,void *skillHandle,nbServer *server){
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
static int *serverCommand(nbCELL context,void *skillHandle,nbServer *server,nbCELL arglist,char *text){
  /* process commands here */
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
static int serverDestroy(nbCELL context,void *skillHandle,nbServer *server){
  nbLogMsg(context,0,'T',"serverDestroy called");
  if(server->socket!=0) serverDisable(context,skillHandle,server);
  nbFree(server,sizeof(nbServer));
  return(0);
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

//================================================================================================
// Peer Skill

/* see - nb_peer.h
typedef struct NB_MOD_NBP_CLIENT{
  nbCELL specCell;          // specification string
  NB_Term *brainTerm;       // brain term for old routines
  struct BRAIN *brain;      // brain structure
  int option;               // 0 - store, 1 - store and forward, 2 - write directly
  struct LISTENER  *ear;    // old listener to support transition - used if queue and schedule specified
  nbIDENTITY self;          // identity to portray for self when connecting
  nbCELL     scheduleCell;  // schedule - (optional when used with queue)
  nbCELL     synapseCell;   // synapse - used to respond to schedule
  } nbClient;
*/

// Check for new queue files
//   This function is scheduled by queueEnable() using nbSynapseOpen()

static void clientAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  nbClient *client=(nbClient *)nodeHandle;
  nbCELL value=nbCellGetValue(context,cell);

  if(value!=NB_CELL_TRUE) return;  // only act when schedule toggles to true
  nbqSend(client->ear->brainTerm);  // send queue to the peer
  }

/*
*  Skull client
*/
static void peerSkullClient(nbCELL context,nbClient *client,char *cursor){
  char buffer[NB_BUFSIZE];

  /* nbpSkull(session,brainTerm); */
  nbLogMsg(context,0,'I',"peerSkullClient calling nbpOpen");
  if((currentSession=nbpOpen(1,client->brainTerm,"@"))==NULL){
    nbLogMsg(context,0,'E',"Unable to open a skull session");
    return;
    }
  nbLogMsg(context,0,'T',"session open");
  while(nbGetReply("< ",buffer,sizeof(buffer))!=NULL && strncmp(buffer,"quit",4)!=0){
    if(nbpPut(currentSession,buffer)!=0){
      nbpClose(currentSession);
      nbLogMsg(context,0,'E',"Skull session failed.");
      return;
      }
    nbLogPut(context,"\n");
    }
  nbpStop(currentSession);
  nbpClose(currentSession);
  }

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*/
static void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbClient *client;
  nbCELL cell=NULL;
  nbSET argSet;
  int type;
  char *brainSpec,portrayIdent[128];

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Expecting string peer specification as first parameter");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string peer specification as first parameter");
    return(NULL);
    }
  brainSpec=nbCellGetString(context,cell);
  client=nbAlloc(sizeof(nbClient));
  // get identity to portray
  client->self=NULL;
  *portrayIdent=0;
  client->brain=nbBrainNew(1,brainSpec);
  if(client->brain==NULL){
    nbLogMsg(context,0,'E',"Peer specification not recognized");
    nbCellDrop(context,cell);
    nbFree(client,sizeof(nbClient));
    return(NULL);
    }
  if(client->brain->id==NULL){
    nbLogMsg(context,0,'E',"Required identity not found in peer specification");
    nbCellDrop(context,cell);
    nbFree(client,sizeof(nbClient));
    return(NULL);
    }
  if(client->brain->myId==NULL) client->brain->myId=client->brain->id;
  client->self=nbIdentityGet(context,client->brain->myId);
  if(client->brain->dir==NULL) client->option=2;       // write directly to socket if queue not specified
  else if(client->brain->hostname==NULL) client->option=0; // store if socket not specified
  else client->option=1;                               // store and forward if socket and queue specified
  //nbLogMsg(context,0,'T',"option=%d based on first argument",client->option);
  client->brainTerm=nbBrainMakeTerm((struct TERM *)context,client->brain);
  client->ear=NULL;
  client->specCell=cell;
  client->scheduleCell=NULL;
  client->synapseCell=NULL;
  cell=nbListGetCell(context,&argSet);              // check for optional schedule cell - not value
  if(cell){
    if(client->brain->dir==NULL){
      nbLogMsg(context,0,'E',"Schedule parameter ignored when queue not specified.");
      }
    else{
      client->option=1;                             // store and use scheduled forward
      nbLogMsg(context,0,'T',"option=%d based on second argument",client->option);
      // create old listener to support transition
      client->ear=nbpListenerNew(context);  // using NBP because we no longer need NBQ object methods
      client->ear->brainTerm=(NB_Term *)nbCellGrab(context,(nbCELL)client->brainTerm);
      if(client->brain->hostname!=NULL) client->ear->dstBrain=(NB_Term *)nbCellGrab(context,(nbCELL)client->brainTerm);
      client->scheduleCell=cell;
      nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
      }
    if(nbListGetCellValue(context,&argSet)!=NULL){
      nbLogMsg(context,0,'E',"The peer skill only accepts two argument.");
      if(client->ear){  // 2012-12-27 eat 0.8.13 - CID 751554
        if(client->ear->brainTerm) nbCellDrop(context,(nbCELL)client->ear->brainTerm);
        nbCellDrop(context,(nbCELL)client->ear);
        }
      nbFree(client,sizeof(nbClient));
      return(NULL);
      }
    }
  return(client);
  }

static int clientShow(nbCELL context,void *skillHandle,nbClient *client,int option){
  if(option!=NB_SHOW_REPORT) return(0);
  nbLogPut(context," using %s ",client->self->name->value);
  nbCellShow(context,(nbCELL)client->self);
  nbLogPut(context,"\n specification: ");
  //nbCellShow(context,(nbCELL)peer->brain);
  printBrain(client->brain);
  nbLogPut(context,"\n");
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    <node>[(0|1|2|3)][:<text>]
*/
static int clientCommand(nbCELL context,void *skillHandle,nbClient *client,nbCELL arglist,char *text){
  char *cursor=text;
  char cmd[NB_BUFSIZE];
  nbCELL cell=NULL;
  nbSET argSet;
  double optionReal;  // 0 - store, 1 - store and forward, 2 - send directly, 3 - skull
  int option=client->option;
  nbIDENTITY clientIdentityStore=nbIdentityGetActive(context);
  
  if(arglist){
    argSet=nbListOpen(context,arglist);
    cell=nbListGetCellValue(context,&argSet);
    if(!cell || nbCellGetType(context,cell)!=NB_TYPE_REAL){
      nbLogMsg(context,0,'E',"Expecting 0, 1, 2, or 3 as first argument");
      return(1);
      }
    optionReal=nbCellGetReal(context,cell);
    nbCellDrop(context,cell);
    if(optionReal!=0 && optionReal!=1 && optionReal!=2 && optionReal!=3){
      nbLogMsg(context,0,'E',"Expecting 0, 1, 2, or 3 as first argument");
      return(1);
      }
    switch(client->option){
      case 0:
        if(optionReal!=0) nbLogMsg(context,0,'E',"Socket not defined for node - option ignored.");
        break;
      case 1:
        option=(int)optionReal;
        break;
      case 2: 
        if(optionReal<2) nbLogMsg(context,0,'E',"Queue not defined for node - option ignored.");
        else option=(int)optionReal;
        break;
      }
    }
  if(client->self) nbIdentitySetActive(context,client->self);  // restore at any exit point
  else nbLogMsg(context,0,'E',"Self identity not defined");
  while(*cursor==' ') cursor++;
  if(option==3){
    peerSkullClient(context,client,cursor);
    nbIdentitySetActive(context,clientIdentityStore);
    return(0);
    }
  if(*cursor!=0){  // don't need to fuss options 0 and 2 stuff for null commands
    /* if we are deadly embraceable spawn a child */
    /* in the future allow an option to declare a peer as not embraceable---never connecting back */
    if(nb_mode_embraceable && option!=0){
      char name[512];
      if(option==2) snprintf(cmd,sizeof(cmd),":%s:%s",nbNodeGetNameFull(context,name,sizeof(name)),text); //dtl: replaced sprintf
      else snprintf(cmd,sizeof(cmd),":%s(%d):%s",nbNodeGetNameFull(context,name,sizeof(name)),option,text); //dtl: replaced sprintf
      nbSpawnSkull(context,NULL,cmd);
      nbIdentitySetActive(context,clientIdentityStore);
      return(0);
      }
    // 2006-05-25 eat - included test for hostname to cover local (unix) domain sockets
    //                  We may want to change this to test for non-NULL ipaddr---local domain sockets have "" ipaddr
    if(option==2 && (((struct BRAIN *)client->brainTerm->def)->port!=0 || ((struct BRAIN *)client->brainTerm->def)->hostname!=NULL)){
      nbpSend(1,client->brainTerm,"",cursor);
      nbIdentitySetActive(context,clientIdentityStore);
      return(0);
      }
    /* assuming \ or / */
    nbqStoreCmd(client->brainTerm,cursor);
    }
  if(option==1){
    nbqSend(client->brainTerm); /* forward */
    }
  nbIdentitySetActive(context,clientIdentityStore);
  return(0);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int clientEnable(nbCELL context,void *skillHandle,nbClient *client){
  if(client->synapseCell!=NULL) return(0);
  client->synapseCell=nbSynapseOpen(context,skillHandle,client,client->scheduleCell,clientAlarm);
  nbLogMsg(context,0,'I',"Enabled %s",nbCellGetString(context,client->specCell));
  nbLogFlush(context);
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
static int clientDisable(nbCELL context,void *skillHandle,nbClient *client){
  if(client->synapseCell==NULL) return(0);
  client->synapseCell=nbSynapseClose(context,client->synapseCell);  // release the synapse
  nbLogMsg(context,0,'I',"Disabled %s",nbCellGetString(context,client->specCell));
  nbLogFlush(context);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
static int clientDestroy(nbCELL context,void *skillHandle,nbClient *client){
  clientDisable(context,skillHandle,client);
  if(client->ear!=NULL) nbCellDrop(context,(nbCELL)client->ear);
  nbCellDrop(context,client->specCell);
  nbFree(client,sizeof(nbClient));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *peerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,clientShow);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,clientDisable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *clientBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_SHOW,clientShow);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,clientDisable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }

//==========================================================================================
// peer.copier skill

// Lookup a brain term for a specified client node identifier

static NB_Term *getClientBrainTerm(nbCELL context,char *label,char *name){
    nbClient *client;
    nbCELL cell;
    int cellType;

    cell=nbTermLocate(context,name);
    if(cell==NULL){
      nbLogMsg(context,0,'E',"%s node '%s' not found",label,name);
      return(NULL);
      }
    cell=nbTermGetDefinition(context,cell);
    cellType=nbCellGetType(context,cell);
    if(cellType!=NB_TYPE_NODE){
      nbLogMsg(context,0,'E',"%s node term '%s' not defined as node",label,name);
      return(NULL);
      }
    client=nbNodeGetKnowledge(context,cell);
    return(client->brainTerm);
    }

/*
*  copier command() method
*
*    <node>:<text>
*    
*    <text>     ::= {a|b|q|c|t} <fileSpec> <fileSpec>
*    <fileSpec> ::= [<client>:]filename
*
*
*/
static int serviceCmdCopy(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL arglist,char *text){
  char *cursor=text;
  char mode;
  NB_Term *srcBrainTerm=NULL,*dstBrainTerm=NULL;
  char cmd[NB_BUFSIZE];
  char *srcNodeName=NULL,*srcFileName,*dstNodeName=NULL,*dstFileName;
  //nbCELL cell;

  while(*cursor==' ') cursor++; 
  mode=*cursor;
  if(strchr("abqctp",mode)==NULL){
    nbLogMsg(context,0,'E',"Expecting mode {a|b|q|c|t} at: %s",cursor);
    return(1);
    }
  while(*cursor && *cursor!=' ') cursor++;
  while(*cursor==' ') cursor++;
  if(strlen(cursor)>sizeof(cmd)-1){
    nbLogMsg(context,0,'E',"Command is too long for buffer");
    return(1);
    }
  strncpy(cmd,cursor,strlen(cursor)+1);
  cursor=cmd;
  srcFileName=cursor;
  while(*cursor && *cursor!=' ' && *cursor!=':' && *cursor!=';') cursor++;
  if(*cursor==':'){
    *cursor=0;
    cursor++;
    srcNodeName=srcFileName;
    srcFileName=cursor;
    while(*cursor && *cursor!=' ' && *cursor!=';') cursor++;
    }
  if(srcFileName==cursor){
    nbLogMsg(context,0,'E',"Source file name not specified.");
    return(1);
    }
  if(*cursor==0){
    nbLogMsg(context,0,'E',"Destination file name not specified.");
    return(1);
    }
  *cursor=0;
  cursor++;
  while(*cursor==' ') cursor++;
  dstFileName=cursor;
  while(*cursor && *cursor!=' ' && *cursor!=':' && *cursor!=';') cursor++;
  if(*cursor==':'){
    *cursor=0;
    cursor++;
    dstNodeName=dstFileName;
    dstFileName=cursor;
    while(*cursor && *cursor!=' ' && *cursor!=';') cursor++;
    }
  if(dstFileName==cursor){
    nbLogMsg(context,0,'E',"Destination file name not specified.");
    return(1);
    }
  if(*cursor==' '){
    *cursor=0;
    cursor++;
    while(*cursor==' ') cursor++;
    }
  if(*cursor==';') *cursor=0;
  else if(*cursor){
    nbLogMsg(context,0,'E',"Extra parameters not recognized at: %s",cursor);
    return(1);
    }
  if(srcNodeName!=NULL){
    srcBrainTerm=getClientBrainTerm(context,"Source",srcNodeName);
    if(srcBrainTerm==NULL) return(1);
    }
  if(dstNodeName!=NULL){
    dstBrainTerm=getClientBrainTerm(context,"Destination",dstNodeName);
    if(dstBrainTerm==NULL) return(1);
    }
  nbpCopy(1,srcBrainTerm,srcFileName,dstBrainTerm,dstFileName,mode);
  return(0);
  }

static int peerCmdIdentify(nbCELL context,void *handle,char *verb,char *text){
  char *cursor=text;
  unsigned int bits;   /* number of bits */
  unsigned char symid,bitsStr[10],identityName[256];
  char *savecursor;
  unsigned char se[512],sd[512],sn[512],s[4096];
  vli2048 e,n,d;
  FILE *file;
#if !defined(WIN32)
  char dirname[512];
#endif
  char filename[512];
  char *userdir;

  savecursor=cursor;
  symid=nbParseSymbol((char *)identityName,&cursor);
  if(symid!='t'){
    nbLogMsg(context,0,'E',"Expecting identity name at [%s].",savecursor);
    return(1);
    }
  if(nbpGetPeerKey((char *)identityName)!=NULL){
    nbLogMsg(context,0,'E',"Identity \"%s\" already defined.",identityName);
    return(1);
    }
  symid=nbParseSymbol((char *)bitsStr,&cursor);
  if(symid=='i'){
    bits=atoi((char *)bitsStr);
    if(bits>1024){
      nbLogMsg(context,0,'E',"Keys greater then 1024 bits not supported.");
      return(1);
      }
    }
  else if(symid==';') bits=64;
  else{
    nbLogMsg(context,0,'E',"Expecting number of bits at [%s].",savecursor);
    return(1);
    }
  pkeGenKey(bits,e,n,d);    /* generate encryption key */
  vliputx(e,(char *)se);
  vliputx(n,(char *)sn);
  vliputx(d,(char *)sd);
  sprintf((char *)s,"%s %s.%s.%s.0;",identityName,se,sn,sd); /* format declaration */
  nbLogPut(context,"%s\n",s);
  userdir=nbGetUserDir();
  sprintf(filename,"%s/nb_peer.keys",userdir);
  if((file=fopen(filename,"a"))==NULL){
#if defined(WIN32)
    nbLogMsg(context,0,'E',"Unable to open %s to append identity.",filename);
    return(1);
    }
#else
    int rc;
    if(strlen(userdir)>=sizeof(dirname)){
      nbLogMsg(context,0,'E',"User directory name too long");
      return(1);
      }
    strcpy(dirname,userdir);
    //strcat(dirname,"/.nb");
    rc=mkdir(dirname,S_IRWXU);
    if(rc<0 || (file=fopen(filename,"a"))==NULL){
      nbLogMsg(context,0,'E',"Unable to open %s to append identity.",filename);
      return(1);
      }
    }
#endif
  fprintf(file,"%s\n",s);
  fclose(file);
#if !defined(WIN32)
  if(chmod(filename,S_IRUSR|S_IWUSR)){  // always correct the file permissions  // 2012-12-27 eat 0.8.13 - CID 751532
    nbLogMsg(context,0,'E',"Unable to set %s file permissions. Should be readable by owner only.",filename);
    }
#endif
  return(0);
  }

static int serviceCmdIdentify(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL arglist,char *text){
  peerCmdIdentify(context,NULL,"peer.identify",text);
  return(0);
  }

static int serviceCmdShow(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL arglist,char *text){
  nbTermPrintGloss(NULL,(nbCELL)brainC);
  return(0);
  }

static int serviceCommand(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL arglist,char *text){
  char *verb,*cursor=text;
  while(*cursor==' ') cursor++;
  verb=cursor;
  while(*cursor && *cursor!=' ') cursor++;
  switch(cursor-verb){
    case 4: 
      if(strncmp(verb,"copy",4)==0) return(serviceCmdCopy(context,skillHandle,nodeHandle,arglist,cursor));
      if(strncmp(verb,"show",4)==0) return(serviceCmdShow(context,skillHandle,nodeHandle,arglist,cursor));
      break;
    case 8:
      if(strncmp(verb,"identify",8)==0) return(serviceCmdIdentify(context,skillHandle,nodeHandle,arglist,cursor));
      break;
    }
  nbLogMsg(context,0,'E',"Command verb not recognized");
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *serviceBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serviceCommand);
  return(NULL);
  }

//================================================================================================
// Skull Skill
//
// This skill is used by the NBP protocol when spawning skull processes, and should
// not be used in rules.  This skill is unusual because it only has a "construct" function
// and the "construct" function does not return.  Instead it takes full control of the
// interpreter.
//
//   nb ":define skull node peer.skull(socket,"<identity>");"
//
// In the future, we'll consider supporting calls to module functions that are not skill methods.
//
//   Example:   +<module>.<skill>(<argList>);
//    
// This would enable the skull skill to be converted to s simple function.
//
//   nb ":+peer.skull(<argList>)"

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*/
static void *skullConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  char *identityName;                   // identity name
  NB_PeerKey *skullIdentity;      // server identity
  nbCELL cell=NULL;
  double socketDouble=0;
  int socket=0;
  nbSET argSet;
  struct NBP_SESSION *session;
  char ipaddr[20];

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_REAL){
    nbLogMsg(context,0,'E',"Expecting socket number as first parameter");
    return(NULL);
    }
  socketDouble=nbCellGetReal(context,cell);
  nbCellDrop(context,cell);
  socket=(int)socketDouble;
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL || nbCellGetType(context,cell)!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string server identity name as second parameter");
    return(NULL);
    }
  identityName=nbCellGetString(context,cell);
  nbCellDrop(context,cell);
  skullIdentity=nbpGetPeerKey(identityName);
  if(skullIdentity==NULL){
    nbLogMsg(context,0,'E',"Skull identity not recognized");
    return(NULL);
    }
  if(nbListGetCellValue(context,&argSet)!=NULL){
    nbLogMsg(context,0,'E',"The skull skill only accepts two parameters.");
    return(NULL);
    }

  /* create a session handle for calling nbp functions */
  if((session=nbpNewSessionHandle(skullIdentity))==NULL) nbExit("skullConstruct unable to obtain a session handle - terminating");
  session->channel->socket=socket;
  nbIpGetSocketIpAddrString(socket,ipaddr);
  snprintf(session->channel->ipaddr,16,"%s",ipaddr); //2012-02-07 dtl: replaced strcpy
  nbLogMsg(context,0,'I',"Reading from %s socket %d",session->channel->ipaddr,session->channel->socket);
  nbLogFlush(context);
  skull_socket=socket;      // set global variable
  nbpServe(NULL,session,1,NULL); // serve an existing socket connection
  return(NULL);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *skullBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,skullConstruct);
  return(NULL);
  }

/*====================================================================================
*
====================================================================================*/
typedef struct NB_MOD_QUEUE{
  nbCELL           nameCell;      // queue name
  nbCELL           scheduleCell;  // schedule 
  nbCELL           synapseCell;   // synapse - used to respond to schedule
  } nbQueue;

// Check for new queue files
//   This function is scheduled by queueEnable() using nbSynapseOpen()

static void queueAlarm(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  nbQueue *queue=(nbQueue *)nodeHandle;
  nbCELL value=nbCellGetValue(context,cell);

  if(value!=NB_CELL_TRUE) return;  // only act when schedule toggles to true
  nbQueueProcess(context,nbCellGetString(context,queue->nameCell),queue->synapseCell);
  }

/*
*  construct() method
*
*    define <term> node <node>[(<args>)][:<text>]
*/
static void *queueConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbQueue *queue;
  nbCELL nameCell=NULL;
  nbCELL scheduleCell=NULL;
  nbCELL nullCell=NULL;
  nbSET argSet;
  int type;

  argSet=nbListOpen(context,arglist);
  nameCell=nbListGetCellValue(context,&argSet);
  if(nameCell==NULL){
    nbLogMsg(context,0,'E',"Expecting string queue name as first parameter.");
    return(NULL);
    }
  type=nbCellGetType(context,nameCell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string queue name as first parameter.");
    return(NULL);
    }
  scheduleCell=nbListGetCell(context,&argSet);  // get schedule cell - not value
  nullCell=nbListGetCellValue(context,&argSet);
  if(nullCell!=NULL){
    nbLogMsg(context,0,'E',"The queue skill only accepts two parameters.");
    return(NULL);
    }

  queue=nbAlloc(sizeof(nbQueue));
  queue->nameCell=nameCell;
  queue->scheduleCell=scheduleCell;
  queue->synapseCell=NULL; 

  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(queue);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int queueEnable(nbCELL context,void *skillHandle,nbQueue *queue){
  if(queue->synapseCell!=NULL) return(0);
  queue->synapseCell=nbSynapseOpen(context,skillHandle,queue,queue->scheduleCell,queueAlarm);
  nbLogMsg(context,0,'I',"Enabled queue %s",nbCellGetString(context,queue->nameCell));
  nbLogFlush(context);
  return(0);
  }

/*
*  disable method
*
*    disable <node>
*/
static int queueDisable(nbCELL context,void *skillHandle,nbQueue *queue){
  if(queue->synapseCell==NULL) return(0);
  queue->synapseCell=nbSynapseClose(context,queue->synapseCell);  // release the synapse
  nbLogMsg(context,0,'I',"Disabled queue %s",nbCellGetString(context,queue->nameCell));
  nbLogFlush(context);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *queueCommand(nbCELL context,void *skillHandle,nbQueue *queue,nbCELL arglist,char *text){
  /* process commands here */
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
static int queueDestroy(nbCELL context,void *skillHandle,nbQueue *queue){
  nbLogMsg(context,0,'T',"queueDestroy called");
  nbCellDrop(context,queue->nameCell);
  nbCellDrop(context,queue->scheduleCell);
  nbCellDrop(context,queue->synapseCell);
  nbFree(queue,sizeof(nbQueue));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *queueBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,queueConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,queueDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,queueEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,queueCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,queueDestroy);
  return(NULL);
  }

//=====================================================================
// Commands

static int peerCmdShow(struct NB_CELL *context,void *handle,char *verb,char *cursor){
  nbTermPrintGloss(NULL,(nbCELL)brainC);
  return(0);
  }

//=====================================================================
/* Module Initialization Function */

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *nbBind(nbCELL context,char *ident,nbCELL arglist,char *text){
  nbVerbDeclare(context,"peer.identify",NB_AUTH_CONTROL,0,NULL,&peerCmdIdentify,"<identity> [bits]");
  nbVerbDeclare(context,"peer.show",NB_AUTH_CONNECT,0,NULL,&peerCmdShow,"");
  return(NULL);
  }
