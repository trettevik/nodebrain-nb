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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbmsg.c
*
* Title:    NodeBrain Message API Functions
*
* Purpose:
*
*   This file provides functions for delivering messages to
*   multiple decoupled peers.  It is intended as a foundation
*   layer for event streams and object replication using an adaptive
*   ring topology.
*
* Synopsis:
*
*   *** include when the functions settle down ***
*
* Description:
*
*   Nodes capable of originating messages are numbered.  Each node
*   generates message numbers that are unique and increasing.
*
*     N-T-C
*     N     - node number
*       T   - UTC time
*         C - wrap around counter 
*      
* 
*   A message is a character string with a prefix identifying
*   the nodes that have already seen the message. We call this prefix
*   a "path".
*
*     N-T-C,N-T-C:...data...
*
*   *** the following requires update ***
*
*   A message log is a flat file containing messages starting in column 2.
*   Column 1 identifies the type of record.  The first line is header identified
*   by 'h' in column 1. The path of the header identifies the "state" at the
*   end of the prior file, and the text provides the name of the prior file.
*   This simplifies walking backwords through the files to get to a particular
*   state. Each subsequent message is identified by 'm' in column 1, and is
*   expected to have increasing message numbers with respect to each node
*   identified in the path.
*
*   By "state" we simply mean a set of highest seen message numbers, one
*   for each node.  In addition to each log file having a beginning state, 
*   the log directory has a state defined by the last log file.  This is the
*   state that will be recorded at the beginning of the next log file.  It
*   can be obtained by reading the last log file from start to end and replacing
*   the message number for each node as encounter in the path of each message.
*
*   Log files within a log directory are named as follows.  The named file
*   points to the active message file, and the numbered files contain the
*   messages.  We sometimes call the named file the "state file" and the
*   numbered files "content files".
*
*     <node>.msg  -> <nnnnnnnn>.msg
*     <nnnnnnnn>.msg
*
*   When content is not required, there are no numbered files and the named
*   file is a regular file.
*   
*   The first line of a message log file, the header, looks like this.
*
*     LL1nNTTTTCCCCNTTTTCCCC...1TTTTCCCC
*     LL - record length
*       1 - record type "header"
*        nNTTTTCCCCNTTTTCCCC... - message path
*        n - number of peer message ids in path
*         N - local node number
*          TTTT - local message time
*              CCCC - local message count
*                  N - peer node number
*                   TTTT - peer message time
*                       CCCC - peer message count
*                           ... - additional peer message ids
*                              1TTTTCCCC - data
*                              1 - data type is binary
*                               TTTT - time of prior message log file
*                                   CCCC - message count of prior message log file
*
*   Subsequent records have the same basic structure, only the record type
*   and data portion are different.
*
*     LL2nNTTTTCCCCNTTTTCCCC...0cccccccccc...0
*       2 - standard message   0 - data type is character string
*                               cccccccccc...0 - null terminated character string
*
*   A message log directory is a subdirectory of the "message" sub directory
*   of a NodeBrain caboodle.  We call a group of colaborating nodebrain agents
*   a "cabal".  The name of a cabal and the node number N are used to complete 
*   a message log directory name.
*
*     CABOODLE/message/CABAL/nN
*
*   In addition to a message log having a "state", defined as a set of message 
*   numbers---the highest seen for each node, a program accessing a message
*   log also has a similar independent state.  This is an important feature
*   that enables a process to start out in a different state than the log it
*   manages, but synchronize over time.
*   
*   The API functions in this file support the reading and writting of messages
*   to and from a message log while maintaining our "clever" notion of state.
*
*==============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2009-12-12 Ed Trettevik (original prototype)
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-05-11 eat 0.8.1  Included msglog fileJumper method for message cache
* 2010-05-21 eat 0.8.2  Modified sequence of switching back from msglog to UDP queue
*            After a dropped packet we now read from the message queue,
*            drain the UDP queue and then read from the message log again.
*            This is intended to avoid a timing situation where there are
*            new messages in the message log but no UDP packet to trigger
*            a read of the message log.
* 2010-05-23 eat 0.8.2  Included logic in nbMsgCabalEnable needed to shutdown extra connections
* 2010-06-07 eat 0.8.2  Included support for cursor mode message log reading
* 2011-01-20 eat 0.8.5  Finishing up support for multiple consumers
* 2011-02-08 eat 0.8.5  Cleaned up non-blocking SSL handshake
* 2011-02-08 eat 0.8.5  Enable source and sink connections to made in either direction
*            Previously sink nodes connected to all source nodes.  Now listeners
*            can be configured on the sink nodes and source nodes will connect
*            to all sink nodes.  This enables a source node to transmit to sink
*            nodes when a firewall prevents the sink from connecting to the
*            source but enables the source to connect to the sink.
*
* 2012-01-04 eat - modified to accept replacement connections
* 2012-01-31 dtl - Checker updates
* 2012-04-22 eat 0.8.8  Included nbMsgLogPrune function
* 2012-04-22 eat 0.8.8  Added a one byte file status field to file header record
*            The file status is used to mark the first file in the set.  This
*            state is assigned to the initial file when a log is created, and
*            set on the new first file when older files are deleted by the
*            nbMsgLogPrune function.
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-27 eat 0.8.13 Checker updates
* 2012-12-31 eat 0.8.13 Checker updates
*==============================================================================
*/
#include <ctype.h>
#include <nb/nbi.h>
#if !defined(WIN32)
#include <sys/un.h>
#endif

/*
*  Define macros for ntohl and htohs to avoid alignment problems on some platforms
*/
//#if defined(SOLARIS)
//#define CNTOHL(_byte) ntohl((uint32_t)((((((*_byte)<<8)|(*(_byte+1)))<<8)|(*(_byte+2)))<<8)|*(_byte+3))
//#define CNTOHS(_byte) ntohs((uint16_t)(*_byte<<8|*(_byte+1)))
//#else
//#define CNTOHL(_byte) ntohl(*(uint32_t *)_byte)
//#define CNTOHS(_byte) ntohs(*(uint16_t *)_byte)
//#endif
#define CNTOHL(_byte) ((uint32_t)((((((*(_byte))<<8)|(*(_byte+1)))<<8)|(*(_byte+2)))<<8)|*(_byte+3))
#define CNTOHS(_byte) ((uint16_t)(*(_byte)<<8|*(_byte+1)))

/*
*  Add a message consumer - UDP socket
*/
int nbMsgConsumerAdd(nbMsgLog *msglog,char *name){
  int  rc;
  nbMsgConsumer *consumer,**consumerP;

  if(strlen(name)>=sizeof(consumer->name)){  // 2012-12-16 eat - CID 751639
    outMsg(0,'E',"nbMsgConsumerAdd: name too long - %s",name);
    return(-1);
    }
  //outMsg(0,'T',"Cabal %s node %s adding consumer %s",msglog->cabal,msglog->nodeName,name);
  for(consumerP=&msglog->consumer;*consumerP!=NULL && (rc=strcmp(name,(*consumerP)->name))>0;consumerP=&(*consumerP)->next);
  if(*consumerP==NULL || rc<0){ // insert if new
    //outMsg(0,'T',"Cabal %s node %s consumer %s is new - inserting",msglog->cabal,msglog->nodeName,name);
    consumer=(nbMsgConsumer *)nbAlloc(sizeof(nbMsgConsumer));
    memset(consumer,0,sizeof(nbMsgConsumer));
    consumer->next=*consumerP;
    strcpy(consumer->name,name);
    if((consumer->socket=socket(AF_UNIX,SOCK_DGRAM,0))<0){
      outMsg(0,'E',"nbMsgConsumerAdd: Unable to get socket - %s\n",strerror(errno));
      nbFree(consumer,sizeof(nbMsgConsumer));
      return(-1);
      }
    consumer->un_addr.sun_family=AF_UNIX;
    snprintf(consumer->un_addr.sun_path,sizeof(consumer->un_addr.sun_path),"message/%s/%s/%s.socket",msglog->cabal,msglog->nodeName,name);
    *consumerP=consumer;
    if(msgTrace) outMsg(0,'I',"Cabal %s node %s consumer %s added as %s",msglog->cabal,msglog->nodeName,name,consumer->un_addr.sun_path);
    }
  //else outMsg(0,'W',"Cabal %s node %s consumer request %s is duplicate",msglog->cabal,msglog->nodeName,name);
  return(0);
  }

/*
*  Remove a message consumer - UDP socket
*/

int nbMsgConsumerRemove(nbMsgLog *msglog,char *name){
  int  rc;
  nbMsgConsumer *consumer,**consumerP;

  outMsg(0,'T',"Cabal %s node %s removing consumer %s",msglog->cabal,msglog->nodeName,name);
  for(consumerP=&msglog->consumer;*consumerP!=NULL && (rc=strcmp(name,(*consumerP)->name))>0;consumerP=&(*consumerP)->next);
  if(*consumerP==NULL || rc<0){
    outMsg(0,'W',"Cabal %s node %s consumer %s not registered - remove request ignored",msglog->cabal,msglog->nodeName,name);
    return(-1);
    }
  consumer=*consumerP;
  *consumerP=consumer->next;
  if(consumer->socket) close(consumer->socket);
  nbFree(consumer,sizeof(nbMsgConsumer));
  return(0);
  }

/*
*  Send message to all registered consumers
*/
int nbMsgConsumerSend(nbCELL context,nbMsgLog *msglog,void *data,int len){
  nbMsgConsumer *consumer,**consumerP;
  //int rc;
  for(consumerP=&msglog->consumer;*consumerP!=NULL;){
    consumer=*consumerP;
    if(msgTrace) outMsg(0,'T',"Cabal %s node %s sending datagram message of length %d to consumer %s",msglog->cabal,msglog->nodeName,len,consumer->name);
    if(msgTrace) outMsg(0,'T',"Socket %d file '%s'",consumer->socket,consumer->un_addr.sun_path);
    if(sendto(consumer->socket,data,len,MSG_DONTWAIT,(struct sockaddr *)&consumer->un_addr,sizeof(struct sockaddr_un))<0){
      nbLogMsg(context,0,'W',"Cabal %s node %s consumer %s: %s",msglog->cabal,msglog->nodeName,consumer->name,strerror(errno));
      // 2011-01-19 eat - remove entry if consumer has gone away
      // may need to check rc and not always remove
      //nbMsgConsumerRemove(msglog,consumer->name);       
      close(consumer->socket);
      *consumerP=consumer->next;
      nbFree(consumer,sizeof(nbMsgConsumer));
      }
    else consumerP=&(*consumerP)->next;
    }
  return(0); 
  }

//==============================================================================
// Operations on state structure 
//==============================================================================

void nbMsgStatePrint(FILE *file,nbMsgState *msgstate,char *title){
  int node;
  nbMsgNum *msgnum;

  fprintf(file,"%s\n",title);
  for(node=0;node<=NB_MSG_NODE_MAX;node++){
    msgnum=&msgstate->msgnum[node];  // 2013-01-01 eat - CID 762051 - NB_MSG_NODE_MAX+1 elements
    if(msgnum->time || msgnum->count){
      fprintf(file,"Node %3.3u time=%10.10u count=%10.10u\n",node,msgnum->time,msgnum->count);
      }
    }
  }

/*
*  Create a state vector
*/
nbMsgState *nbMsgStateCreate(nbCELL context){
  nbMsgState *state;
  state=(nbMsgState *)nbAlloc(sizeof(nbMsgState));
  memset(state,0,sizeof(nbMsgState));
  return(state);
  }

void nbMsgStateFree(nbCELL context,nbMsgState *msgState){
  nbFree(msgState,sizeof(nbMsgState));
  }

/*
*  Set a state for an individual node
*/
int nbMsgStateSet(nbMsgState *state,unsigned char node,uint32_t time,uint32_t count){
  // if(node<0 || node>NB_MSG_NODE_MAX){ // 2012-12-16 eat - CID 751587
  if(node>=NB_MSG_NODE_MAX){
    fprintf(stderr,"nbMsgStateSet: Node %d out of range\n",node);
    return(-1);
    }
  state->msgnum[node].time=time;
  state->msgnum[node].count=count;
  return(0);
  }

/*
*  Set a state for an individual node
*
*  Returns: 
*    -1 - nbMsgId is not 1 greater than the state - state not updated
*     0 - nbMsgId is 1 greate than the state - state was updated
*/
int nbMsgStateSetFromMsgId(nbCELL context,nbMsgState *state,nbMsgId *msgid){
  int node=msgid->node;
  uint32_t time=CNTOHL(msgid->time);
  uint32_t count=CNTOHL(msgid->count);
  uint32_t countAhead=state->msgnum[node].count+1;
  if(countAhead!=1){
    if(!countAhead) countAhead++;  // skip over zero - is special count value
    nbLogMsg(context,0,'T',"nbMsgStateSetFromMsgId: node=%d countAhead=%u count=%u",node,countAhead,count);
    if(countAhead!=count) return(-1);
    }
  state->msgnum[node].time=time;
  state->msgnum[node].count=count;
  return(0);
  }

/*
*  Compare wrap-around counter
*
*  Returns: -1 a<b, 0 a==b, 1 a>b
*/
int nbMsgCountCompare(uint32_t a,uint32_t b){
  if(a==b) return(0);
  if(a>0xc0000000 && b<0x3fffffff) return(-1);
  if(b>0xc0000000 && a<0x3fffffff) return(1);
  if(a<b) return(-1);
  return(1);
  }

/*
*  Compare nbMsgId to nbMsgState
*
*  Returns:
*    -1 nbMsgId<nbMsgState 
*     0 nbMsgId==nbMsgState
*     1 nbMsgId>nbMsgState
*/
int nbMsgStateCheck(nbMsgState *state,nbMsgId *msgid){
  int node=msgid->node;
  uint32_t time=CNTOHL(msgid->time);
  uint32_t count=CNTOHL(msgid->count);
  if(time<state->msgnum[node].time) return(-1);
  if(time>state->msgnum[node].time) return(1);
  return(nbMsgCountCompare(count,state->msgnum[node].count));
  }


//==============================================================================
// Operations on message records
//==============================================================================

int nbMsgPrintHex(FILE *file,unsigned short len,unsigned char *buffer){
  unsigned char *cursor=buffer;
  char hexbuf[4096],*hexcur=hexbuf,*hexend=hexbuf+sizeof(hexbuf)-2;
  char *hexchar="0123456789ABCDEF";

  if(len<1) return(-1);
  for(cursor=buffer;cursor<buffer+len;cursor++){
    //fprintf(file,"%2.2x",*cursor);
    if(hexcur>=hexend){
      *hexcur=0;
      fprintf(file,"%s",hexbuf);
      hexcur=hexbuf;
      }
    sprintf(hexcur,"%c%c",*(hexchar+(*cursor>>4)),*(hexchar+(*cursor&0x0f)));
    hexcur+=2;
    }
  if(hexcur==hexbuf) return(0);
  *hexcur=0;
  fprintf(file,"%s",hexbuf);
  return(0);
  }

/*
*  Print a binary message record to a file
*
*  This function may be used for debugging, or to convert
*  a message log file into human readable form.  We
*  attempt to represent even "corrupted" records in a
*  format that represents the record so it can be corrected
*  and converted back to binary form.
*
*  If a length field is bad, fixing a corrupted file may
*  be difficult. 
*
*   msglen,msgtype,datatype,peerMsgIds,N-T-C,...:data
*
*   msgtype  - (s)tate, (h)eader, (m)essage, (e)xpres message, (?)
*   datatype - (n)one, (c)haracter, (b)inary, (?)
*     character data represented as char
*     all other represented in hex
*
*  Returns:
*     0 - success
*     1 - record format error
*/
int nbMsgPrint(FILE *file,nbMsgRec *msgrec){
  unsigned int mNode,mTime,mCount;
  nbMsgId *msgid;
  unsigned short int msglen;
  unsigned char mType,mDataType;
  unsigned char *msgcur,*msgend,*cursor;
  int msgids;
  int rc=0;

  msglen=(msgrec->len[0]<<8)|msgrec->len[1]; 
  // make sure it is long enough to have msgids field and msgid values it claims
  if(msglen<sizeof(nbMsgRec) || msglen<sizeof(nbMsgRec)+msgrec->msgids*sizeof(nbMsgId)){
    fprintf(stderr,"nbMsgPrint: msglen=%d\n",msglen);
    fprintf(file,"?x:");
    if(msglen) nbMsgPrintHex(file,msglen,msgrec->len);
    fprintf(file,"\n");
    return(1);
    }
  msgend=msgrec->len+msglen;
  switch(msgrec->type){
    case NB_MSG_REC_TYPE_STATE: mType='s'; break;
    case NB_MSG_REC_TYPE_HEADER: mType='h'; break;
    case NB_MSG_REC_TYPE_MESSAGE: mType='m'; break;
    case NB_MSG_REC_TYPE_FOOTER: mType='f'; break;
    default: mType='?',rc=1;
    }
  switch(msgrec->datatype){
    case NB_MSG_REC_DATA_NONE: mDataType='n'; break;
    case NB_MSG_REC_DATA_CHAR: mDataType='c'; break;
    case NB_MSG_REC_DATA_BIN:  mDataType='b'; break;
    case NB_MSG_REC_DATA_ID:   mDataType='i'; break;
    default: mDataType='?',rc=1;
    }
  fprintf(file,"%u%c%u%c%u",msglen,mType,msgrec->type,mDataType,msgrec->datatype);
  msgid=&msgrec->si;  // start with state msgid and display all msgid values
  for(msgids=msgrec->msgids+2;msgids;msgids--){
    mNode=msgid->node;
    mTime=CNTOHL(msgid->time);
    mCount=CNTOHL(msgid->count);
    fprintf(file,",%u-%u-%u",mNode,mTime,mCount);
    msgid++;
    }
  fprintf(file,":");
  msgcur=(unsigned char *)msgid;
  if(msgcur<msgend){
    if(msgrec->datatype==NB_MSG_REC_DATA_CHAR){
      for(cursor=msgcur;cursor<msgend && isprint(*cursor);cursor++);
      if(cursor!=msgend-1 || *cursor!=0){
        rc=1; // string contains unprintable characters
        fprintf(file,"x:");
        nbMsgPrintHex(file,msgend-msgcur,msgcur);
        }
      else fprintf(file,"c:%s",msgcur);
      }
    else if(msgrec->datatype==NB_MSG_REC_DATA_ID){
      mNode=msgid->node;
      mTime=CNTOHL(msgid->time);
      mCount=CNTOHL(msgid->count);
      fprintf(file,"%u-%u-%u",mNode,mTime,mCount);
      msgid++;
      if((unsigned char *)msgid>=msgend) fprintf(file,"?00");
      else fprintf(file,",%2.2x",*(char *)msgid);
      }
    else{
      fprintf(file,"x:");
      nbMsgPrintHex(file,msgend-msgcur,msgcur);
      }
    }
  fprintf(file,"\n");
  //fprintf(stderr,"msgcur=%p msgend=%p\n",msgcur,msgend);
  return(rc);
  }

/*
*  Update log and program state and recommend action
*
*    This function looks at scans the msgid values in a message record
*    (path or state) and updates the log and pgm state vectors.
*    It also sets the dataP to the address of the data portion
*    of the message.  The return code identifies the relationship
*    of the message relative to both the log and pgm.
*    
* Return Code: 
*
*    -1 - message corrupted - format error
*
*     Otherwise return code is bit mask - see NB_MSG_STAT_*
*
*     1 - message is new to pgm state
*     2 - message is new to log state
*     4 - sequence low - record has already been processed 
*     8 - sequence high - record count advanced by more than 1 
*
*     Codes provided by nbMsgLogRead
*
*    16 - end of file (file closed - nbMsgLogRead will open next file on next read
*    32 - end of log  (file closed - nbMsgLogRead will open and seek to last position before read)
*
*   textP is used to set a pointer to the text portion of the message 
*/
int nbMsgLogSetState(nbCELL context,nbMsgLog *msglog,nbMsgRec *msgrec){
  uint32_t node,tranTime,tranCount,recordTime,recordCount;
  int logStateFlag=0,pgmStateFlag=0;
  nbMsgId *msgid;
  nbMsgState *logState=msglog->logState;
  nbMsgState *pgmState=msglog->pgmState;
  int rc;

  msglog->state&=0xffffffff-(NB_MSG_STATE_LOG|NB_MSG_STATE_PROCESS|NB_MSG_STATE_SEQLOW|NB_MSG_STATE_SEQHIGH);
  if(msgrec->pi.node==msglog->node){ // called while reading own log
    msgid=&msgrec->pi;
    recordTime=CNTOHL(msgid->time);
    recordCount=CNTOHL(msgid->count);
    if(recordCount!=msglog->recordCount+1){  // make sure record count increments by 1
      if((rc=nbMsgCountCompare(recordCount,msglog->recordCount))<=0){
        // if(recordTime>msglog->recordTime)...error
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogSetState: Record sequence low");
        msglog->state|=NB_MSG_STATE_SEQLOW;
        return(NB_MSG_STATE_SEQLOW);
        }
      else{
        // if(recordTime<msglog->recordTime)...error
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogSetState: Record sequence high");
        msglog->state|=NB_MSG_STATE_SEQHIGH;
        return(NB_MSG_STATE_SEQHIGH);
        }
      }
    msglog->recordTime=recordTime;
    msglog->recordCount=recordCount;
    }
  msgid=&msgrec->si;
  if(msgrec->type==NB_MSG_REC_TYPE_FOOTER){
    msglog->state|=NB_MSG_STATE_FILEND;
    return(NB_MSG_STATE_FILEND);
    }
  node=msgid->node;
  tranTime=CNTOHL(msgid->time);
  tranCount=CNTOHL(msgid->count);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogSetState: tranTime=%u,tranCount=%u,LOG,node=%u,stateTime=%u,stateCount=%u",tranTime,tranCount,node,logState->msgnum[node].time,logState->msgnum[node].count);
  if(tranTime>logState->msgnum[node].time){
    logState->msgnum[node].time=tranTime;
    logState->msgnum[node].count=tranCount;
    logStateFlag|=NB_MSG_STATE_LOG;
    }
  else if(tranTime==logState->msgnum[node].time){
    if(tranCount>logState->msgnum[node].count){
      logState->msgnum[node].count=tranCount;
      logStateFlag|=NB_MSG_STATE_LOG;
      }
    }
  if(pgmState!=logState){
    if(tranTime>pgmState->msgnum[node].time){
      pgmState->msgnum[node].time=tranTime;
      pgmState->msgnum[node].count=tranCount;
      pgmStateFlag|=NB_MSG_STATE_PROCESS;
      }
    else if(tranTime==pgmState->msgnum[node].time){
      if(tranCount>pgmState->msgnum[node].count){
        pgmState->msgnum[node].count=tranCount;
        pgmStateFlag|=NB_MSG_STATE_PROCESS;
        }
      }
    }
  else if(logStateFlag&NB_MSG_STATE_LOG) pgmStateFlag|=NB_MSG_STATE_PROCESS;
  //msgid+=2+msgrec->msgids;
  msglog->state|=pgmStateFlag&logStateFlag;
  return(pgmStateFlag|logStateFlag);
  }

/*
*  Check message log file state relative to program/process state
*   
*    This function is used to determine if the log state at the
*    start of a message log file is far enough back to include
*    the state of the program/process, pgmState. 
*
*    The pgm state is included if the msgid for all nodes specified
*    in the message log file header state are less than or equal
*    to the msgid in the program state.
*
*  Returns: 0 - does not satisfy, 1 - satisfies 
*/
int nbMsgIncludesState(nbMsgLog *msglog){
  unsigned int node,mTime,mCount;
  int msgids;
  nbMsgRec *msgrec=(nbMsgRec *)msglog->msgrec;
  nbMsgId *msgid=&msgrec->si;
  //nbMsgState *logState=msglog->logState;
  nbMsgState *pgmState=msglog->pgmState;

  if(msgTrace) outMsg(0,'T',"nbMsgIncludesState: called - false if no message saying it is true");
  node=msgid->node;
  mTime=CNTOHL(msgid->time);
  mCount=CNTOHL(msgid->count);
  if(mTime>pgmState->msgnum[node].time) return(0);
  // 2011-01-25 eat - changed from logState to pgmState - was wrong before
  //else if(mTime==logState->msgnum[node].time && nbMsgCountCompare(mCount,logState->msgnum[node].count)>0) return(0);
  else if(mTime==pgmState->msgnum[node].time && nbMsgCountCompare(mCount,pgmState->msgnum[node].count)>0) return(0);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    node=msgid->node;
    mTime=CNTOHL(msgid->time);
    mCount=CNTOHL(msgid->count);
    if(mTime>pgmState->msgnum[node].time) return(0);
    else if(mTime==pgmState->msgnum[node].time && nbMsgCountCompare(mCount,pgmState->msgnum[node].count)>0) return(0);
    msgid++;
    }
  if(msgTrace) outMsg(0,'T',"nbMsgIncludesState: true");
  return(1);  
  }

/*
*  Store a message id in network byte order
*/
void nbMsgIdStuff(nbMsgId *msgid,unsigned char node,uint32_t mTime,uint32_t mCount){  
  msgid->node=node;                               // 2012-12-31 eat - VID 5374,4611-0.8.13-1  node from int to unsigned char
  msgid->time[3]=mTime%256; mTime=mTime>>8;	  // 2012-12-31 eat - VID 4726-0.8.13-1  &0xff to %256
  msgid->time[2]=mTime%256; mTime=mTime>>8;       // 2012-12-31 eat - VID 4610-0.8.13-1
  msgid->time[1]=mTime%256; mTime=mTime>>8;       // 2012-12-31 eat - VID 5480-0.8.13-1
  msgid->time[0]=mTime%256;                  
  msgid->count[3]=mCount%256; mCount=mCount>>8;   // 2012-12-31 eat - VID 5129-0.8.13-1
  msgid->count[2]=mCount%256; mCount=mCount>>8;   // 2012-12-31 eat - VID 5448-0.8.13-1
  msgid->count[1]=mCount%256; mCount=mCount>>8;   // 2012-12-31 eat - VID 4848-0.8.13-1
  msgid->count[0]=mCount%256;
  }

/*
*  Generate state record to tell a replicator what we need
*
*    This function creates a state record identifying the lesser
*    of the log and pgm state for each node.  We may need to think
*    about the case where the pgm state has a non zero time for a
*    given node, but the log state has a zero time.  For now we
*    assume the log needs to get synchronized, so we go with the
*    zero value and will start pulling the peer's log from the
*    start.  The nbMsgLogWriteReplica() function tells the program
*    which messages it needs to process.
*
*  Returns: length of record
*/
int nbMsgLogStateToRecord(nbCELL context,nbMsgLog *msglog,unsigned char *buffer,int buflen){
  nbMsgRec *msgrec=(nbMsgRec *)buffer;
  nbMsgId  *msgid;
  nbMsgState *logState=msglog->logState;
  nbMsgState *pgmState=msglog->pgmState;
  int nodeIndex;
  uint32_t mTime,mCount;
  int node=msglog->node;
  int len;

  nbLogMsg(context,0,'T',"nbMsgLogStateToRecord: &msglog=%p &buffer=%p buflen=%d",msglog,buffer,buflen);
  msgrec->type=NB_MSG_REC_TYPE_STATE;
  msgrec->datatype=NB_MSG_REC_DATA_NONE;
  msgrec->msgids=0;
  msgid=&msgrec->si;
  nbMsgIdStuff(msgid,node,logState->msgnum[node].time,logState->msgnum[node].count);
  msgid++;
  // We have a phony path message id here.  Need to stuff from something in msglog structure 
  nbMsgIdStuff(msgid,node,logState->msgnum[node].time,logState->msgnum[node].count);
  msgid++;
  for(nodeIndex=0;nodeIndex<NB_MSG_NODE_MAX;nodeIndex++){
    if(nodeIndex==msglog->node) continue; // we handled self above
    if(logState->msgnum[nodeIndex].time<pgmState->msgnum[nodeIndex].time){
      mTime=logState->msgnum[nodeIndex].time; 
      mCount=logState->msgnum[nodeIndex].time;
      } 
    else if(logState->msgnum[nodeIndex].time>pgmState->msgnum[nodeIndex].time){
      mTime=pgmState->msgnum[nodeIndex].time; 
      mCount=pgmState->msgnum[nodeIndex].time;
      }
    else{
      mTime=logState->msgnum[nodeIndex].time; 
      // compare wrap around counter
      if(nbMsgCountCompare(logState->msgnum[nodeIndex].count,pgmState->msgnum[nodeIndex].count)<=0)
        mCount=logState->msgnum[nodeIndex].count;
      else mCount=pgmState->msgnum[nodeIndex].count;
      }
    if(mTime!=0){ // stuff msgid
      msgrec->msgids++;
      nbMsgIdStuff(msgid,nodeIndex,mTime,mCount);
      msgid++;
      }
    }  
  len=(char *)msgid-(char *)msgrec;
  msgrec->len[0]=len<<8;
  msgrec->len[1]=len&0xff;
  if(msgTrace){
    nbLogMsg(context,0,'T',"nbMsgLogStateToRecord: Created state record");
    nbMsgStatePrint(stderr,msglog->logState,"Log state:");
    nbMsgStatePrint(stderr,msglog->pgmState,"Pgm state:");
    nbMsgPrint(stderr,msgrec);
    nbLogMsg(context,0,'T',"nbMsgLogStateToRecord: returning");
    }
  return(len);
  }

int nbMsgStateSetFromRecord(nbCELL context,nbMsgState *msgstate,nbMsgRec *msgrec){
  nbMsgId *msgid;
  int msgids;
  int nodeIndex;

  msgid=&msgrec->si;
  nodeIndex=msgid->node;
  msgstate->msgnum[nodeIndex].time=CNTOHL(msgid->time);
  msgstate->msgnum[nodeIndex].count=CNTOHL(msgid->count);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    nodeIndex=msgid->node;
    msgstate->msgnum[nodeIndex].time=CNTOHL(msgid->time);
    msgstate->msgnum[nodeIndex].count=CNTOHL(msgid->count);
    msgid++;
    }
  return(0);
  }

nbMsgState *nbMsgLogStateFromRecord(nbCELL context,nbMsgRec *msgrec){
  nbMsgState *msgstate;
  nbMsgId *msgid;
  int msgids;
  int nodeIndex;

  msgstate=nbMsgStateCreate(context);
  // here we could just call nbMsgStateSetFromRecord(context,msgstate,msgrec)
  msgid=&msgrec->si;
  nodeIndex=msgid->node;
  msgstate->msgnum[nodeIndex].time=CNTOHL(msgid->time);
  msgstate->msgnum[nodeIndex].count=CNTOHL(msgid->count);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    nodeIndex=msgid->node;
    msgstate->msgnum[nodeIndex].time=CNTOHL(msgid->time);
    msgstate->msgnum[nodeIndex].count=CNTOHL(msgid->count);
    msgid++;
    }
  if(msgTrace){
    nbMsgStatePrint(stderr,msgstate,"Client state:");
    nbMsgPrint(stderr,msgrec);
    }
  return(msgstate);
  }



/* 
*  Extract information from header record
*
*  Returns:
*
*    NULL - all is well
*    point to message if error
*
*    node
*    utime
*    count
*    text pointer
*/
char *nbMsgHeaderExtract(nbMsgRec *msgrec,int node,uint32_t *tranTimeP,uint32_t *tranCountP,uint32_t *recordTimeP,uint32_t *recordCountP,uint32_t *fileTimeP,uint32_t *fileCountP,char *flags){
  nbMsgId *msgid=&msgrec->si;
  int msglen;

  if(msgTrace) nbMsgPrint(stderr,msgrec);
  if(msgrec->type!=NB_MSG_REC_TYPE_HEADER) return("msg type not header");
  if(msgrec->datatype!=NB_MSG_REC_DATA_ID) return("msg data type not ID");
  if(msgid->node!=node) return("state message id node does not match expected node");
  *tranTimeP=CNTOHL(msgid->time);
  *tranCountP=CNTOHL(msgid->count);
  msgid++;
  if(msgid->node!=node) return("log message id node does not match expected node");
  *recordTimeP=CNTOHL(msgid->time);
  *recordCountP=CNTOHL(msgid->count);
  msgid+=1+msgrec->msgids;
  if(msgid->node!=node) return("log message id node does not match expected node");
  *fileTimeP=CNTOHL(msgid->time);
  *fileCountP=CNTOHL(msgid->count);
  msgid++;  // see if we have flags
  msglen=(msgrec->len[0]<<8)|msgrec->len[1];
  if((char *)msgid>=((char *)msgrec)+msglen) *flags=0;
  else *flags=*(char *)msgid;  // return flags 
  return(NULL);
  }

/*
*  Locate data area of message record
*/
void *nbMsgData(nbCELL context,nbMsgRec *msgrec,int *datalen){
  void *data;
  int msglen,prefixlen;

  msglen=(msgrec->len[0]<<8)|msgrec->len[1];
  prefixlen=sizeof(nbMsgRec)+msgrec->msgids*sizeof(nbMsgId);
  data=(void *)msgrec+prefixlen;
  *datalen=msglen-prefixlen;
  return(data);
  }

//==============================================================================
// Reading message log files
//==============================================================================

/*
*  Read the next message record
*
*    This function reads message log files in order.  The return
*    code is a bit mask as defined by NB_MSG_STATE_*.  This function
*    extends the codes returned by nbMsgLogSetState.
*
*    A log writer and log reader both read at least one message log file
*    when they first open a message log.  However they respond differently
*    to the NB_MSG_STATE_LOGEND state.  A log writer switches to producer mode
*    by calling nbMsgLogProduce and then starts writing using nbMsgLogWriteString
*    nbMsgLogWriteData or nbMsgLogWriteReplica.  A log reader may periodically
*    attempt to read again by calling nbMsgLogRead.  If the log file has grown
*    nbMsgLogRead will return more messages.  Otherwise it will return another
*    NB_MSG_STATE_LOGEND condition.
*
*  Returns:
*
*    NB_MSG_STATE_LOGEND  - at end of log - read again to follow
*    NB_MSG_STATE_FILEND  - at end of file - read again to step to next file
*    NB_MSG_STATE_PROCESS - message is new to process state
*    -1 - error
*/
int nbMsgLogRead(nbCELL context,nbMsgLog *msglog){
  unsigned char *cursor;
  unsigned short int msglen;
  unsigned char *bufend;
  unsigned char *readbuf;
  int readlen;
  unsigned short int partlen=0;
  char filename[64];
  int msgbuflen;
  off_t pos;
  time_t utime;
  struct stat filestat;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Called for msglog=%p state=0x%x fileCount=%u fileOffset=%u filesize=%u",msglog,msglog->state,msglog->fileCount,msglog->fileOffset,msglog->filesize);
  if(msglog->state&NB_MSG_STATE_LOGEND){
    sprintf(msglog->filename,"%10.10d.msg",msglog->fileCount);  
    sprintf(filename,"message/%s/%s/%s",msglog->cabal,msglog->nodeName,msglog->filename);
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Check for growth in cabal \"%s\" node %u file %s",msglog->cabal,msglog->node,msglog->filename);
    if(msglog->file){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Logic error - cabal \"%s\" node %u file %s - still open while log is in end-of-log state",msglog->cabal,msglog->node,msglog->filename);
      return(-1);
      }
    if(msglog->fileOffset!=msglog->filesize){
      nbLogMsg(context,0,'L',"nbMsgLogRead: Logic error - cabal \"%s\" node %u file %s offset %d is not equal filesize=%u after LOGEND",msglog->cabal,msglog->node,filename,msglog->fileOffset,msglog->filesize);
      exit(1);
      }
    if((msglog->file=open(filename,O_RDONLY))<0){ // 2013-01-01 eat - CID 762053 - moved ahead of lstat - time of check time of use (TOCTOU)
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to open file %s - %s",filename,strerror(errno));
      return(-1);
      }
    if(lstat(filename,&filestat)){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to stat file %s - %s",filename,strerror(errno));
      return(-1);
      }
    if(filestat.st_size<msglog->filesize){
      nbLogMsg(context,0,'L',"nbMsgLogRead: Logic error - cabal \"%s\" node %u file %s size %d is less than msglog->filesize=%u",msglog->cabal,msglog->node,filename,filestat.st_size,msglog->filesize);
      exit(1);
      }
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: msglog->fileOffset=%u msglog->filesize=%u",msglog->fileOffset,msglog->filesize);
    if((pos=lseek(msglog->file,msglog->filesize,SEEK_SET))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to seek file %s to offset %u - %s",filename,msglog->filesize,strerror(errno));
      return(-1);
      }
    if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to read file %s - %s",filename,strerror(errno));
      return(-1);
      }
    if(msgbuflen==0){
      close(msglog->file);
      msglog->file=0;
      return(NB_MSG_STATE_LOGEND);
      }
    msglog->filesize+=msgbuflen;
    msglog->msgbuflen=msgbuflen;
    msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
    msglog->state&=0xff-NB_MSG_STATE_LOGEND;
    }
  else if(msglog->state&NB_MSG_STATE_FILEND){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Stepping to next file");
    if(msglog->file){
      nbLogMsg(context,0,'L',"nbMsgLogRead: Logic error - file %s still open while log is in eof state\n",msglog->filename);
      return(-1);
      }
    time(&utime);
    msglog->fileTime=utime;
    msglog->fileCount++;
    sprintf(msglog->filename,"%10.10d.msg",msglog->fileCount);  
    sprintf(filename,"message/%s/%s/%s",msglog->cabal,msglog->nodeName,msglog->filename);
    if((msglog->file=open(filename,O_RDONLY))<0){ 
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to open file %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to read file %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    msglog->filesize=msgbuflen;
    msglog->msgbuflen=msgbuflen;
    msglog->state&=0xff-NB_MSG_STATE_FILEND;
    // do something here to validate header relative to log state - have a validate function
    cursor=(unsigned char *)msglog->msgbuf;
    msglog->fileOffset=(*cursor<<8)|*(cursor+1); // set file offset just past header
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Starting %s msglog->fileOffset=%u",msglog->filename,msglog->fileOffset);
    cursor+=(*cursor<<8)|*(cursor+1);  // step to next record - over header
    msglog->msgrec=(nbMsgRec *)cursor;
    }
  else{
    cursor=(unsigned char *)msglog->msgrec;
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Step msglog=%p to next record at %p *cursor=%2.2x%2.2x",msglog,cursor,*cursor,*(cursor+1));
    msglog->fileOffset+=(*cursor<<8)|*(cursor+1);  // update file offset
    cursor+=(*cursor<<8)|*(cursor+1);  // step to next record
    msglog->msgrec=(nbMsgRec *)cursor;
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: After step msglog->fileCount=%u msglog->fileOffset=%u msglog->filesize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
    if(msglog->fileOffset>msglog->filesize){
      nbLogMsg(context,0,'L',"nbMsgLogRead: Logic error - fileOffset>filesize - terminating");
      exit(1);
      }
    }
  bufend=msglog->msgbuf+msglog->msgbuflen;
  cursor=(unsigned char *)msglog->msgrec;
  if(cursor>bufend){ // 2012-12-16 eat - CID 751556
    nbLogMsg(context,0,'L',"nbMsgLogRead: Logic error - cursor beyond bufend - terminating");
    exit(1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: At next record %p bufend=%p msgbuf=%p msgbuflen=%d",msglog->msgrec,bufend,msglog->msgbuf,msglog->msgbuflen);
  if(cursor+sizeof(nbMsgRec)>bufend || (msglen=(*cursor<<8)|*(cursor+1))>bufend-cursor){  // see if we need
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Reading cabal \"%s\" node %d file %s into buffer",msglog->cabal,msglog->node,msglog->filename);
    partlen=bufend-cursor;
    if(partlen) memcpy(msglog->msgbuf,cursor,partlen);
    readbuf=msglog->msgbuf+partlen;
    readlen=NB_MSG_BUF_LEN-partlen;
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: readbuf=%p readlen=%d",readbuf,readlen);
    if((readlen=read(msglog->file,readbuf,readlen))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to read cabal \"%s\" node %d - %s",msglog->cabal,msglog->node,strerror(errno));
      return(-1);
      }
    else if(readlen==0){  // eof
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: End of file reached",cursor);
      if((pos=lseek(msglog->file,0,SEEK_CUR))<0){
        nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to seek file %s - %s",filename,strerror(errno));
        return(-1);
        }
      if(pos!=msglog->filesize){ // check for logic error
        nbLogMsg(context,0,'E',"nbMsgLogRead: Logic error - cabal \"%s\" node %d file %s - file size mismatch",msglog->cabal,msglog->node,filename);
        return(-1);
        }
      close(msglog->file);
      msglog->file=0;
      msglog->state|=NB_MSG_STATE_LOGEND;
      return(NB_MSG_STATE_LOGEND);
      }
    msglog->filesize+=readlen;
    msglog->msgbuflen=partlen+readlen;
    msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
    bufend=msglog->msgbuf+msglog->msgbuflen; // recompute bufend
    }
  if(msgTrace){
    nbLogMsg(context,0,'T',"nbMsgLogRead: printing");
    nbMsgPrint(stderr,msglog->msgrec);
    }
  if(msglog->msgrec->type==NB_MSG_REC_TYPE_FOOTER){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Footer encountered");
    cursor=(unsigned char *)msglog->msgrec;
    if(cursor+(*cursor<<8)+*(cursor+1)!=bufend){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Corrupted message log file cabal \"%s\" node %d file %s - footer found before file end",msglog->cabal,msglog->node,msglog->filename);
      return(-1);
      }
    if((readlen=read(msglog->file,msglog->msgbuf,1))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to read cabal \"%s\" node %d - %s",msglog->cabal,msglog->node,strerror(errno));
      return(-1);
      }
    if(readlen){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Corrupted message log file cabal \"%s\" node %d file %s - footer found before file end",msglog->cabal,msglog->node,msglog->filename);
      return(-1);
      }
    close(msglog->file);
    msglog->file=0;
    msglog->state|=NB_MSG_STATE_FILEND;
    return(NB_MSG_STATE_FILEND);
    }
  else if(msglog->msgrec->type>2){
    nbLogMsg(context,0,'L',"nbMsgLogRead: Invalid record type %d - terminating",msglog->msgrec->type);
    exit(1);
    }
  if(msglog->mode==NB_MSG_MODE_CURSOR && nbMsgLogCursorWrite(context,msglog)<0){
    nbLogMsg(context,0,'T',"nbMsgLogRead: Unable to update cursor for cabal '%s' node '%s' - terminating",msglog->cabal,msglog->nodeName);
    exit(1);
    }
  return(nbMsgLogSetState(context,msglog,msglog->msgrec));
  }

/*
*  Open message log for reading
*
*    This function opens a message log and positions by time.  We first check to
*    see if the caller has provided a specific message log file name.
*    If this file does not exist, we open <nodename>.msg and chain back as far as necessary
*    to open the file containing the requested time. 
*
*    When calling this function to replicate messages to a peer B in a given state,
*    you should set openTime to B's state relative to the node A from which you are
*    replicating.  The address of the first message record is returned.  This message
*    and subsequent messages must be filtered using peer B's state.
*
*  mode:
*
*    NB_MSG_MODE_CONSUMER  - normal log reader (e.g. message.server)
*    NB_MSG_MODE_SINGLE    - single file reader - file basename passed as parameter
*    NB_MSG_MODE_PRODUCER  - writer - will switch to writing via nbMsgLogProduce()
*    NB_MSG_MODE_SPOKE     - writer that doesn't send UDP packets (for spoke nodes)
*    NB_MSG_MODE_CURSOR    - reader that reads from a cursor file to establish the
*                            the starting position and updates the cursor file for
*                            every message read.  This is used by consumers that
*                            have no other means of storing their state.  When this
*                            mode is used, the pgmState must be NULL. 
*
*  pgmState:
*
*    May be initialized by the caller, in which case it may be different than the
*    log state.  If a NULL value is given for pgmState, then it will be the same
*    as the log state.  The API functions recognize when the log state and pgmState
*    are the same.  One of the impacts is that a client mode peer will not be passed
*    anything from the log at startup.  The log will still be read at startup to
*    initialize the state. 
*
*  basename:
*
*    For SINGLE mode this is the basename of the file to be read.  For CURSOR
*    mode it is the basename of the cursor file.
*
*/
nbMsgLog *nbMsgLogOpen(nbCELL context,char *cabal,char *nodeName,unsigned char node,char *basename,int mode,nbMsgState *pgmState){
  nbMsgLog *msglog;
  char filename[128];
  char cursorFilename[128];
  unsigned short int msglen;
  int msgbuflen;
  char *errStr;
  uint32_t tranTime,tranCount,recordTime,recordCount,fileTime,fileCount;
  char fileState;            // flag bits on file header/footer
  char linkname[128];
  char linkedname[32];
  int linklen;
  nbMsgCursor msgcursor;
  int flags;
  struct stat filestat;      // file statistics
  int option=0;              // more flags

  flags=mode;
  mode&=0xff;

  if(pgmState && mode==NB_MSG_MODE_CURSOR){
    nbLogMsg(context,0,'L',"nbMsgLogOpen: Cabal '%s' node '%s' open with non-null pgmState incompatible with cursor mode - terminating",cabal,nodeName);
    nbLogFlush(context);
    exit(1);
    }
  if((!basename || !*basename) && (mode==NB_MSG_MODE_CURSOR || mode==NB_MSG_MODE_SINGLE)){
    nbLogMsg(context,0,'L',"nbMsgLogOpen: Cabal '%s' node '%s' open with null basename incompatible with cursor or single mode - terminating",cabal,nodeName);
    nbLogFlush(context);
    exit(1);
    }
  // 2012-12-31 eat - dropping test because always false now that node is unsigned char
  //if(node>NB_MSG_NODE_MAX){
  //  outMsg(0,'E',"nbMsgLogOpen: Node number %d exceeds limit of %d",node,NB_MSG_NODE_MAX);
  //  return(NULL);
  //  }
  if(strlen(cabal)>sizeof(msglog->cabal)-1){
    outMsg(0,'E',"nbMsgLogOpen: Message cabal name length exceeds %d bytes",(int)sizeof(msglog->cabal)-1);
    return(NULL);
    }
  if(strlen(nodeName)>sizeof(msglog->nodeName)-1){
    outMsg(0,'E',"nbMsgLogOpen: Message node name length exceeds %d bytes",(int)sizeof(msglog->nodeName)-1);
    return(NULL);
    }
  // Get the file name to start with
  if(mode==NB_MSG_MODE_SINGLE){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: called with single file option");
    if(strlen(basename)>sizeof(linkedname)-1){
      fprintf(stderr,"nbMsgLogOpen: file name length %d exceeds limit of %d\n",(int)strlen(basename),(int)sizeof(linkedname)-1);
      return(NULL);
      }
    strcpy(linkedname,basename);
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: linkedname=%s",linkedname);
    }
  else{
    snprintf(linkname,sizeof(linkname),"message/%s/%s/%s.msg",cabal,nodeName,nodeName); //2012-01-16 dtl replaced sprintf
    if(lstat(linkname,&filestat)<0){
      outMsg(0,'E',"nbMsgLogOpen: Unable to stat %s - %s",linkname,strerror(errno));
      return(NULL);
      }
    if(S_ISLNK(filestat.st_mode)){
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: State file %s is a symbolic link",linkname);
      // change this to retry in a loop for a few times if not mode PRODUCER because nbMsgLogFileCreate may be removing and creating link
      linklen=readlink(linkname,linkedname,sizeof(linkedname));
      if(linklen<0){
        outMsg(0,'E',"nbMsgLogOpen: Unable to read link %s - %s",linkname,strerror(errno));
        return(NULL);
        }
      if(linklen==sizeof(linkedname)){
        outMsg(0,'E',"nbMsgLogOpen: Symbolic link %s point to file name too long for buffer",linkname);
        return(NULL);
        }
      *(linkedname+linklen)=0;
      option=NB_MSG_OPTION_CONTENT;
      if(msgTrace) nbLogMsg(context,0,'T',"Cabal %s node %s content link is %s -> %s",cabal,nodeName,linkname,linkedname);
      }
    else if(S_ISREG(filestat.st_mode)){
      outMsg(0,'T',"nbMsgLogOpen: State file %s is a regular file",linkname);
      sprintf(linkedname,"%s.msg",nodeName);
      option=NB_MSG_OPTION_STATE; // set option for using regular file for state only - no messages
      outMsg(0,'I',"Cabal %s node %s state file is %s",cabal,nodeName,linkname);
      }
    else{
      outMsg(0,'E',"nbMsgLogOpen: Expecting %s to be a symbolic link or regular file",linkname);
      return(NULL);
      }
    }
  // create a msglog structure
  msglog=(nbMsgLog *)nbAlloc(sizeof(nbMsgLog));
  memset(msglog,0,sizeof(nbMsgLog));
  strncpy(msglog->cabal,cabal,sizeof(msglog->cabal));             // 2012-12-31 eat - VID 4733-0.8.13-1 FP checked above, but changed anyway
  *(msglog->cabal+sizeof(msglog->cabal)-1)=0;
  strncpy(msglog->nodeName,nodeName,sizeof(msglog->nodeName));    // 2012-12-31 eat - VID 5324-0.8.13-1 FP checked above, but changed anyway
  *(msglog->nodeName+sizeof(msglog->nodeName)-1)=0;
  msglog->node=node;
  strcpy(msglog->filename,linkedname);
  if(basename && *basename) strncpy(msglog->consumerName,basename,sizeof(msglog->consumerName));  
  else strncpy(msglog->consumerName,nodeName,sizeof(msglog->consumerName));   // 2012-12-31 eat - VID 5273-0.8.13-1 FP but changed
  *(msglog->consumerName+sizeof(msglog->consumerName)-1)=0;
  msglog->option=option;
  msglog->mode=mode;
  msglog->state=NB_MSG_STATE_INITIAL;
  //msglog->file=0;
  //msglog->maxfilesize=0;
  //msglog->fileTime=0;
  //msglog->fileCount=0;
  //msglog->fileOffset=0;
  //msglog->filesize=0;
  if(mode==NB_MSG_MODE_CONSUMER || mode==NB_MSG_MODE_CURSOR) msglog->synapse=nbSynapseOpen(context,NULL,msglog,NULL,nbMsgLogPoll);
  msglog->logState=nbMsgStateCreate(context);
  if(pgmState) msglog->pgmState=pgmState;
  else msglog->pgmState=msglog->logState;
  msglog->msgbuflen=0; // no data in buffer yet
  msglog->msgbuf=(unsigned char *)nbAlloc(NB_MSG_BUF_LEN);
  //msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  msglog->msgrec=NULL;  // no record yet
  //  }
  // If in CURSOR mode, we position by cursor if cursor file exists.
  if(mode==NB_MSG_MODE_CURSOR){
    sprintf(cursorFilename,"message/%s/%s/%s.cursor",cabal,nodeName,basename);
    if((msglog->cursorFile=open(cursorFilename,O_RDWR))<0){ // have a cursor file 
      nbLogMsg(context,0,'I',"Cursor file '%s' not found. Assuming offset of zero.",cursorFilename);
      }
    else{
      if(lseek(msglog->cursorFile,0,SEEK_SET)<0){
        nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to seek file %s to offset %u - %s",cursorFilename,0,strerror(errno));
        return(NULL);
        }
      if((msgbuflen=read(msglog->cursorFile,(void *)&msgcursor,(size_t)sizeof(msgcursor)))<0){
        nbLogMsg(context,0,'E',"nbMsgLogopen: Unable to read file %s - %s",cursorFilename,strerror(errno));
        return(NULL);
        }
      if(msgbuflen!=sizeof(msgcursor)){
        nbLogMsg(context,0,'E',"nbMsgLogopen: Cursor file '%s' is corrupted read=%d expecting %d - terminating",cursorFilename,msgbuflen,(int)sizeof(msgcursor));
        exit(1);
        }
      msglog->fileCount=msgcursor.fileCount;
      msglog->fileOffset=msgcursor.fileOffset;
      msglog->recordTime=msgcursor.recordTime;
      msglog->recordCount=msgcursor.recordCount;
      nbLogMsg(context,0,'I',"Cabal %s node %s using cursor fileCount=%u fileOffset=%u",msglog->cabal,msglog->nodeName,msglog->fileCount,msglog->fileOffset);
      msglog->filesize=msgcursor.fileOffset;
      msglog->state|=NB_MSG_STATE_LOGEND;
      return(msglog);
      }
    }

  sprintf(filename,"message/%s/%s/%s",cabal,nodeName,msglog->filename);
  if((msglog->file=open(filename,O_RDONLY))<0){ 
    fprintf(stderr,"nbMsgLogOpen: Unable to open file %s - %s\n",filename,strerror(errno));
    nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);
    }

  // If not in CURSOR mode, or in cursor mode without a cursor, we need to position by state.
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: Reading header record cabal \"%s\" node %u fildes=%d file %s",msglog->cabal,msglog->node,msglog->file,filename);
  if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
    fprintf(stderr,"nbMsgLogOpen: Unable to read file %s - %s\n",filename,strerror(errno));
    nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: msgbuflen=%d",msgbuflen);
  //msglog->filesize+=msgbuflen;
  msglog->filesize=msgbuflen;
  msglog->msgbuflen=msgbuflen;
  msglen=CNTOHS(msglog->msgbuf);
  msglog->fileOffset=msglen;
  if(msgTrace) outMsg(0,'T',"nbMsgLogOpen: initial msglog->fileOffset=%u",msglog->fileOffset);
  msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  errStr=nbMsgHeaderExtract(msglog->msgrec,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount,&fileState); 
  if(errStr){
    fprintf(stderr,"nbMsgLogOpen: Corrupted file %s - %s\n",filename,errStr);
    nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);  
    }
  msglog->fileTime=fileTime;
  msglog->fileCount=fileCount;
  msglog->recordTime=recordTime;
  msglog->recordCount=recordCount;
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: Extracted header time=%u count=%u fileTime=%u fileCount=%u",recordTime,recordCount,fileTime,fileCount);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: msglog=%p msglog->logState=%p msglog->pgmState=%p flags=%x",msglog,msglog->logState,msglog->pgmState,flags);
  if(mode!=NB_MSG_MODE_SINGLE && msglog->logState!=msglog->pgmState && !(flags&NB_MSG_MODE_LASTFILE)) while(!(fileState&NB_MSG_FILE_STATE_FIRST) && !nbMsgIncludesState(msglog)){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: File %s does not include requested state",filename);
    close(msglog->file);
    msglog->filesize=0;
    sprintf(filename,"message/%s/%s/%10.10u.msg",cabal,nodeName,fileCount-1);
    if((msglog->file=open(filename,O_RDONLY))<0){ 
      fprintf(stderr,"nbMsgLogOpen: Unable to open file %s - %s\n",filename,strerror(errno));
      nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
      fprintf(stderr,"nbMsgLogOpen: Unable to read file %s - %s\n",filename,strerror(errno));
      nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    msglog->filesize+=msgbuflen;
    msglog->msgbuflen=msgbuflen;
    //fprintf(stderr,"msglog->msgbuflen=%d\n",msglog->msgbuflen);
    msglen=CNTOHS(msglog->msgbuf);
    msglog->fileOffset=msglen;
    fprintf(stderr,"nbMsgLogOpen: 2 msglog->fileOffset=%u\n",msglog->fileOffset);
    msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
    errStr=nbMsgHeaderExtract(msglog->msgrec,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount,&fileState);
    if(errStr){
      fprintf(stderr,"nbMsgLogOpen: Corrupted file %s - %s\n",filename,errStr);
      nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    msglog->fileTime=fileTime;
    msglog->fileCount=fileCount;
    msglog->recordTime=recordTime;
    msglog->recordCount=recordCount;
    }
  if(mode==NB_MSG_MODE_CURSOR){
    msgcursor.fileCount=msglog->fileCount;
    msgcursor.fileOffset=msglog->fileOffset;    // point over header
    msgcursor.recordTime=msglog->recordTime;   
    msgcursor.recordCount=msglog->recordCount;   
    if(msgTrace) outMsg(0,'T',"nbMsgLogOpen: 3 msglog->fileOffset=%u",msglog->fileOffset);
    if((msglog->cursorFile=open(cursorFilename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))<0){ // have a cursor file 
      nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to create file '%s'",cursorFilename);
      nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    if(write(msglog->cursorFile,(void *)&msgcursor,sizeof(msgcursor))<0){
      nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to write cursor to file %s - %s\n",filename,strerror(errno));
      nbFree(msglog->msgbuf,NB_MSG_BUF_LEN);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    }
  // 2011-01-26 eat - set the log state using the first header
  else{ 
    if(msgTrace){
      nbLogMsg(context,0,'T',"nbMsgLogOpen: setting log state using first header");
      nbMsgPrint(stderr,msglog->msgrec); 
      nbMsgStatePrint(stderr,msglog->logState,"Log state before:");
      nbMsgStatePrint(stderr,msglog->pgmState,"Pgm state before:");
      }
    nbMsgStateSetFromRecord(context,msglog->logState,(nbMsgRec *)msglog->msgrec);
    if(msgTrace){
      nbMsgStatePrint(stderr,msglog->logState,"Log state after:");
      nbMsgStatePrint(stderr,msglog->pgmState,"Pgm state after:");
      }
    }
  msglog->fileOffset=0; // reset for now because nbMsgLogRead will step over header 
  // Include code here to figure out what to do if the log doesn't satisfy the requested state
  // For example, if it doesn't go back far enough for a given node, we shouldn't provide any
  // messages for that node.
  sprintf(msglog->filename,"%10.10u.msg",msglog->fileCount);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogOpen: Returning open message log file %s",filename);
  return(msglog); 
  }

int nbMsgLogClose(nbCELL context,nbMsgLog *msglog){
  if(msglog->socket){
    nbListenerRemove(context,msglog->socket);
#if defined(WIN32)
    closesocket(msglog->socket);
#else
    close(msglog->socket);
#endif
    msglog->socket=0;
    }
  if(msglog->cursorFile){
    close(msglog->cursorFile);
    msglog->cursorFile=0;
    }
  return(0);
  }

/*
*  Update message log cursor
*/
int nbMsgLogCursorWrite(nbCELL context,nbMsgLog *msglog){
  nbMsgCursor msgcursor;

  msgcursor.fileCount=msglog->fileCount;
  msgcursor.fileOffset=msglog->fileOffset;
  msgcursor.recordTime=msglog->recordTime;
  msgcursor.recordCount=msglog->recordCount;
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogCursorWrite: msgcursor.fileOffset=%u msglog->fileOffset=%u",msgcursor.fileOffset,msglog->fileOffset);
  if(lseek(msglog->cursorFile,0,SEEK_SET)<0){
    nbLogMsg(context,0,'E',"nbMsgLogCursorWrite: Unable to seek cursor file %d to offset %u - %s",msglog->cursorFile,0,strerror(errno));
    return(-1);
    }
  if(write(msglog->cursorFile,(void *)&msgcursor,sizeof(msgcursor))<0){
    nbLogMsg(context,0,'E',"nbMsgLogCursorWrite: Unable to write cursor to file %d - %s\n",msglog->cursorFile,strerror(errno));
    return(-1);
    }
  return(0);
  }


/*
*  Process messages from msglog
*
*    Here we are expecting a zero return code from the handler and will terminate
*    if we get a non-zero return code.  
*
*  Returns:
*    int>=0  - Number of messages processed
*    exit(1) - non-zero return code from message handler
*    
*/
int nbMsgLogProcess(nbCELL context,nbMsgLog *msglog){
  int processed=0;
  int state;
  int rc;

  while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
    if(msgTrace){
      nbLogMsg(context,0,'T',"nbMsgLogProcess: msglog=%p return from nbMsgLogRead state=0x%x",msglog,state);
      nbMsgPrint(stderr,msglog->msgrec);
      }
    if(state&NB_MSG_STATE_PROCESS){  // handle new message records
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogProcess: msglog=%p calling message handler",msglog);
      if((rc=(*msglog->handler)(context,msglog->handle,msglog->msgrec))!=0){
        nbLogMsg(context,0,'I',"UDP message handler return code=%d",rc);
        exit(1);
        }
      processed++;
      }
    else if(state&NB_MSG_STATE_FILEND){  // if we cross over a file boundary, call the fileJump handler
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogProcess: msglog=%p file end calling fileJumper exit if defined",msglog);
      if(msglog->fileJumper) (*msglog->fileJumper)(context,msglog->handle,(msglog->msgrec->len[0]<<8)+msglog->msgrec->len[1]);
      }
    else{  // no tolerance for error here
      nbLogMsg(context,0,'T',"nbMsgLogProcess: not processing record - state=%d - terminating",state);
      exit(1);
      }
    }
  return(processed);
  }

/*
*  Subscribe to a msglog (consumer requesting UDP feed from producer)
*/
int nbMsgLogSubscribe(nbCELL context,nbMsgLog *msglog,char *name){
  int  sd;                             // udp socket for sending message to the consumer
  struct sockaddr_un un_addr;          // unix domain socket address

  if((sd=socket(AF_UNIX,SOCK_DGRAM,0))<0){
    outMsg(0,'E',"nbMsgLogSubscribe: Unable to get socket - %s\n",strerror(errno));
    return(-1);
    }
  un_addr.sun_family=AF_UNIX;
  snprintf(un_addr.sun_path,sizeof(un_addr.sun_path),"message/%s/%s/~.socket",msglog->cabal,msglog->nodeName);
  if(msgTrace) outMsg(0,'T',"Cabal %s node %s sending subscription '%s' to producer",msglog->cabal,msglog->nodeName,name);
  if(sendto(sd,name,strlen(name)+1,MSG_DONTWAIT,(struct sockaddr *)&un_addr,sizeof(struct sockaddr_un))<0){
    nbLogMsg(context,0,'W',"Cabal %s node %s consumer %s subscription: %s",msglog->cabal,msglog->nodeName,name,strerror(errno));
    close(sd);
    return(-1);
    }
  close(sd);
  //outMsg(0,'T',"nbMsgLogSubscribe: returning");
  return(0);
  }

/*
*  Poll message log.
*
*    This is a scheduled routine to process a message log.  We use this for cases where one or more UDP
*    packets are lost, but no additional packets alert us to that fact.
*
*  
*/
void nbMsgLogPoll(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  nbMsgLog *msglog=(nbMsgLog *)nodeHandle;
  int processed;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogPoll: called");
  processed=nbMsgLogProcess(context,msglog);
  //nbSynapseSetTimer(context,msglog->synapse,0);  // 2011-02-07 eat - troubleshooting a duplicate timer
  if(processed){
    nbMsgLogSubscribe(context,msglog,msglog->consumerName); // renew subscription
    nbSynapseSetTimer(context,msglog->synapse,5);
    }
  else nbSynapseSetTimer(context,msglog->synapse,60);  // this should not be required - an experiment
  }

/*
*  Read incoming packets for message handler
*
*/
void nbMsgUdpRead(nbCELL context,int serverSocket,void *handle){
  nbMsgLog *msglog=(nbMsgLog *)handle;
  unsigned char *buffer=msglog->msgbuf;  // we can use the msglog buffer while the message log file is closed
  size_t buflen=NB_MSG_BUF_LEN;
  int  len,msglen;
  //unsigned short rport;
  //unsigned char daddr[40],raddr[40];
  //unsigned int sourceAddr;
  nbMsgCursor *msgudp=(nbMsgCursor *)buffer;
  nbMsgRec *msgrec=(nbMsgRec *)(buffer+sizeof(nbMsgCursor));
  int state;
  int rc;
  int limit=500; // limit the number of messages handled at one time
  int processed;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: Called at fileCount=%u fileOffset=%u fileSize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
  nbSynapseSetTimer(context,msglog->synapse,0);  // cancel the timer
  len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
  while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
  if(len<0){
    nbLogMsg(context,0,'T',"nbMsgUdpRead: first recvfrom len=%d - errno=%d %s - terminating",len,errno,strerror(errno));
    exit(1);
    }
  else while(len>0){
    if(msgTrace){
      nbLogMsg(context,0,'T',"Datagram len=%d",len);
      nbLogDump(context,buffer,len);
      nbLogMsg(context,0,'T',"nbMsgUdpRead: udp packet fileCount=%u fileOffset=%u",msgudp->fileCount,msgudp->fileOffset); 
      }
    msglen=CNTOHS(((unsigned char *)msgrec));
    if(msglen+sizeof(nbMsgCursor)!=len){
      nbLogMsg(context,0,'E',"packet len=%d not the same as msglen=%u+%u - terminating",len,msglen,sizeof(nbMsgCursor));
      exit(1);
      }
    if(msgTrace){
      nbLogMsg(context,0,'T',"nbMsgUdpRead: recvfrom len=%d",len);
      nbMsgPrint(stderr,msgrec);
      }
    state=nbMsgLogSetState(context,msglog,msgrec);
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: state=%d",state);
    while(len>0 && state&NB_MSG_STATE_SEQLOW){
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: Ignoring message already seen");
      len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
      while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
      if(len>0) state=nbMsgLogSetState(context,msglog,msgrec);
      }
    while(len>0 && state&NB_MSG_STATE_SEQHIGH){
      nbLogMsg(context,0,'T',"nbMsgUdpRead: UDP packet lost - reading from message log");
      if(!(msglog->state&NB_MSG_STATE_LOGEND)){
        nbLogMsg(context,0,'L',"nbMsgUdpRead: Udp packet lost when not in LOGEND state - terminating");
        exit(1);
        }
      processed=nbMsgLogProcess(context,msglog);
      if(!processed){
        nbLogMsg(context,0,'L',"nbMsgUdpRead: Lost message not found in message log - terminating");
        exit(1);
        }
      limit-=processed;
      // drain the udp queue
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: flushing UDP stream - sd=%d",msglog->socket);
      state|=NB_MSG_STATE_SEQLOW;
      while(len>0 && state&NB_MSG_STATE_SEQLOW){
        len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
        while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
        if(len>0) state=nbMsgLogSetState(context,msglog,msgrec);
        }
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: UDP stream flushed");
      }
    if(len>0){
      if(msgudp->fileCount>msglog->fileCount){
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: before new file msglog->fileCount=%u msglog->fileOffset=%u msglog->filesize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
        msglog->fileCount=msgudp->fileCount;
        msglog->filesize=msgudp->fileOffset;
        msglog->fileOffset=msgudp->fileOffset;
        // call fileJumper with offset adjusted to start of first message in new file
        if(msglog->fileJumper) (*msglog->fileJumper)(context,msglog->handle,msglog->fileOffset-((msgrec->len[0]<<8)+msgrec->len[1]));
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: after new file msglog->fileCount=%u msglog->fileOffset=%u msglog->filesize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
        }
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: calling nbMsgCacheInsert");
      if((rc=(*msglog->handler)(context,msglog->handle,msgrec))!=0){
        nbLogMsg(context,0,'I',"UDP message handler return code=%d - terminating",rc);
        exit(1);
        }
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: before update msglog->fileCount=%u msglog->fileOffset=%u msglog->filesize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
      msglog->filesize=msgudp->fileOffset;    // adjust file size to next message location
      msglog->fileOffset=msgudp->fileOffset;  // adjust file offset to next message location
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: after update msglog->fileCount=%u msglog->fileOffset=%u msglog->filesize=%u",msglog->fileCount,msglog->fileOffset,msglog->filesize);
      if(msglog->mode==NB_MSG_MODE_CURSOR && nbMsgLogCursorWrite(context,msglog)<0){
        nbLogMsg(context,0,'T',"Unable to update cursor for cabal '%s' node '%s' - terminating",msglog->cabal,msglog->nodeName);
        exit(1);
        }
      limit--;
      }
    if(limit<=0){ // we can't spend all our time filling the cache - need a chance to send also
      nbLogMsg(context,0,'T',"nbMsgUdpRead: hit limit of uninterrupted reads - returning to allow writes");
      nbSynapseSetTimer(context,msglog->synapse,5);
      return; 
      }
    len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
    while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
    }
  if(len==-1 && errno!=EAGAIN){
    nbLogMsg(context,0,'E',"nbMsgUdpRead: recvfrom error - %s",strerror(errno));
    }
  nbSynapseSetTimer(context,msglog->synapse,5);
  }

/*
*  Start consuming messages
*/
int nbMsgLogConsume(nbCELL context,nbMsgLog *msglog,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
  char filename[512];
  int state;
  int len;
  int fd;

  // may be able to remove this and just let nbMsgUdpRead() handle it
  while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsume: return from nbMsgLogRead state=%d",state);
    if(state&NB_MSG_STATE_PROCESS){  // handle new message records
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsume: calling message handler");
      if((*handler)(context,handle,msglog->msgrec)){
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsume: None zero return from message handler during initial reading of message log");
        return(-1);
        }
      }
    else if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsume: not processing record - state=%d",state);
    }
  // Set up and register listener for UDP socket connections
  len=snprintf(filename,sizeof(filename),"message/%s/%s/%s.socket",msglog->cabal,msglog->nodeName,msglog->consumerName);
  if(len<0 || len>=sizeof(filename)){     // 2012-12-31 eat - VID 5270-0.8.13-1
    *(filename+sizeof(filename)-1)=0;
    nbLogMsg(context,0,'E',"nbMsgLogConsume: Unable to open udp server socket %s - file name too larg for buffer",filename);
    return(-1);
    }
  fd=nbIpGetUdpServerSocket(context,filename,0);    // 2012-12-31 eat - VID 5270-0.8.13-1
  if(fd<0){
    nbLogMsg(context,0,'E',"nbMsgLogConsume: Unable to open udp server socket %s",filename);
    return(-1);
    }
  msglog->socket=fd;
  msglog->handle=handle;
  msglog->handler=handler;
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsumer: set fd=%d to non-blocking",msglog->socket);
  if(fcntl(msglog->socket,F_SETFL,fcntl(msglog->socket,F_GETFL)|O_NONBLOCK)){ // make it non-blocking // 2012-12-27 eat 0.8.13 - CID 761520
    nbLogMsg(context,0,'E',"nbMsgLogConsume: Unable to make udp server socket %s non-blocking - %s",filename,strerror(errno));
    return(-1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogConsumer: F_GETFL return=%d",fcntl(msglog->socket,F_GETFL));
  nbListenerAdd(context,msglog->socket,msglog,nbMsgUdpRead);
  // maintain a status - if producer is not listening, we can try again when the timer pops
  nbSynapseSetTimer(context,msglog->synapse,5);
  nbLogMsg(context,0,'I',"Listening for UDP datagrams as %s",filename);
  // subscribe to UDP packets with request to ~.socket (producer) 
  nbMsgLogSubscribe(context,msglog,msglog->consumerName);
  return(0);
  }

//==============================================================================
// Write message log files
//==============================================================================

/*
*  Open a message log for writing
*
*    To open a message log for writing it must first be
*    open for reading and at end of file on the current
*    message log file.  This establishes the log state
*    at end of file before the file is opened for appending.
*
*    In some use cases a writer will need to synchronize
*    the pgm state when it first starts, which means it has
*    to read the message log file anyway, so reading the
*    message log before switching to write mode is not a
*    burden.
*
*    In other cases it may seem like an inefficient way to
*    start writing.  However, it enables us to ensure the
*    message count is properly sequenced without having to
*    use another file to keep track of the count.  Proper
*    sequence of the message count is important for detecting
*    errors.  For efficiency, it is best to have a single
*    persistent process that does all the writing so the
*    initial read is done only once at startup.
*/


int nbMsgLogEmpty(nbCELL context,char *cabal,char *nodeName){
  return(0);
  }

/*
*  Write a state header record to an open file 
*/
int nbMsgFileWriteState(nbCELL context,int file,nbMsgState *msgstate,unsigned char node,uint32_t recordTime,uint32_t recordCount,uint32_t fileCount,unsigned char fileState){                      // 2012-12-31 eat - VID 4943-0.8.13-1 node to unsigned char,  VID 4846-0.8.13-1 fileState to unsigned char
  nbMsgRec *msgrec;
  nbMsgId *msgid;
  unsigned char msgids;
  size_t msglen;    // 2012-12-31 eat - VID
  int nodeIndex;
  time_t utime;
  uint32_t fileTime;
  int rc;

  if(msgTrace) outMsg(0,'T',"nbMsgFileWriteState: called 1 node=%d",node);
  msgrec=(nbMsgRec *)nbAlloc(sizeof(nbMsgRec)+256*sizeof(nbMsgId));
  msgrec->type=NB_MSG_REC_TYPE_HEADER;
  msgrec->datatype=NB_MSG_REC_DATA_ID;
  msgid=&msgrec->si;
  nbMsgIdStuff(msgid,node,msgstate->msgnum[node].time,msgstate->msgnum[node].count);  // 2012-12-31 eat - VID 4943-0.8.13-1
  msgid++;
  nbMsgIdStuff(msgid,node,recordTime,recordCount);
  msgid++;
  msgids=0;
  for(nodeIndex=0;nodeIndex<32;nodeIndex++){
    if(nodeIndex!=node && msgstate->msgnum[nodeIndex].time!=0){
      msgids++;
      nbMsgIdStuff(msgid,nodeIndex,msgstate->msgnum[nodeIndex].time,msgstate->msgnum[nodeIndex].count)
;
      msgid++;
      }
    }
  time(&utime);
  fileTime=utime;
  nbMsgIdStuff(msgid,node,fileTime,fileCount);
  msgid++;
  *(unsigned char *)msgid=fileState;  // 2012-04-22 eat - include file state - modified by message log file management  // 2012-12-31 eat - VID 4846,5046-0.8.13-1
  msgrec->msgids=msgids;
  msglen=(char *)msgid-(char *)msgrec+1;  // +1 is for fileState   // 2012-12-31 eat - VID 5039,5206-0.8.13-1 msglen from int to size_t
  msgrec->len[0]=msglen>>8;               
  msgrec->len[1]=msglen%256;              // 2012-12-31 eat - VID 5043-0.8.13-1  &0xff to %256
  rc=write(file,msgrec,msglen);
  nbFree(msgrec,sizeof(nbMsgRec)+256*sizeof(nbMsgId));
  return(rc);
  }

/*
*  Create a regular message file with null message state - this is a state file
*/
int nbMsgFileCreateRegular(nbCELL context, char *cabal,char *nodeName,int node){
  char filename[256];
  int file;
  nbMsgState *msgstate;

  sprintf(filename,"message/%s/%s/%s.msg",cabal,nodeName,nodeName);
  if((file=open(filename,O_WRONLY|O_CREAT,S_IRWXU|S_IRGRP))<0){  
    outMsg(0,'E',"nbMsgFileCreateRegular: Unable to create file %s\n",filename);
    return(-1);
    }
  msgstate=nbMsgStateCreate(context);
  if(nbMsgFileWriteState(context,file,msgstate,node,0,0,0,NB_MSG_FILE_STATE_FIRST)<0){
    nbLogMsg(context,0,'E',"nbMsgFileCreateRegular: Unable to write state header to file %s - %s\n",filename,strerror(errno));
    close(file);
    nbMsgStateFree(context,msgstate);
    return(-1);
    }
  close(file);
  nbMsgStateFree(context,msgstate);
  outMsg(0,'I',"Message state file created for cabal '%s' node '%s' as instance %d",cabal,nodeName,node);
  return(0);
  }

/*
*  Create a symbolic linked message file with null state - this is a content file
*/
int nbMsgFileCreateLinked(nbCELL context, char *cabal,char *nodeName,int node){
  char *filebase="0000000001.msg";
  char filename[256];
  char linkname[256];
  int file;
  nbMsgState *msgstate;

  // start a new file
  sprintf(linkname,"message/%s/%s/%s.msg",cabal,nodeName,nodeName);
  sprintf(filename,"message/%s/%s/%s",cabal,nodeName,filebase);
  if((file=open(filename,O_WRONLY|O_CREAT,S_IRWXU|S_IRGRP))<0){ 
    outMsg(0,'E',"nbMsgFileCreateLinked: Unable to create file %s\n",filename);
    return(-1);
    }
  if(symlink(filebase,linkname)<0){
    outMsg(0,'E',"nbMsgFileCreateLinked: Unable to create symbolic link %s to %s - %s\n",linkname,filename,strerror(errno));
    close(file);  // 2012-12-18 eat - CID 751601
    return(-1);
    }
  msgstate=nbMsgStateCreate(context);
  if(nbMsgFileWriteState(context,file,msgstate,node,0,0,1,NB_MSG_FILE_STATE_FIRST)<0){
    outMsg(0,'E',"nbMsgLogFileCreate: Unable to write state header to file %s - %s\n",filename,strerror(errno));
    close(file);
    nbMsgStateFree(context,msgstate);
    return(-1);
    }
  close(file);
  nbMsgStateFree(context,msgstate);
  outMsg(0,'I',"Message content file created for cabal '%s' node '%s' as instance %d",cabal,nodeName,node);
  return(0);
  }

/*
*  When implementing these conversion functions, first attempt communcation with
*  the udp socket interface of the producer.  If the producer is listening, send
*  a transaction to have the producer make the switch.  That way the producer will
*  be able to keep on writing updates.  If the producer is not listening, then
*  the message log can be opened and the switch made by the command utility.
*/
int nbMsgLogConvert2Regular(nbCELL context, char *cabal,char *nodeName,int node){
  return(0);
  }

int nbMsgLogConvert2Linked(nbCELL context, char *cabal,char *nodeName,int node){
  return(0);
  }

/*
*  Initialize a message log
*
*  option:
*
*    0 - Create regular state file <nodeName>.msg
*    1 - Create Symbolic link state file <nodeName>.msg to regular message content file (nnnnnnnn.msg)
*    2 - Convert to regular file (state)
*    3 - Convert to symbolic link (content)
*    4 - Empty to regular file (state)
*    5 - Empty to symbolic link (content);
*
*    See NB_MSG_INIT_OPTION_*
*/
int nbMsgLogInitialize(nbCELL context,char *cabal,char *nodeName,unsigned char node,int option){
  char filename[128];
  struct stat filestat;
  int len;
  
  if(msgTrace) outMsg(0,'T',"nbMsgLogInitialize: called with cabal=%s nodename=%s nodenum=%d option=%d",cabal,nodeName,node,option);
  if(option<0 || option>5){
    outMsg(0,'L',"nbMsgLogInitialize: Invalid option %d - Must be 0 to 5\n",option);
    return(-1);
    }
  len=snprintf(filename,sizeof(filename),"message/%s",cabal);   // 2012-12-31 eat - VID 605-0.8.13-1 Intentional
  if(len<0 || len>=sizeof(filename)){
    *(filename+sizeof(filename)-1)=0;
    outMsg(0,'E',"nbMsgLogInitialize: File name exceeds buffer limit - %s",filename);
    return(-1);
    }
  if(lstat(filename,&filestat)<0){  // if cabal directory not found, create it or find out what's wrong
    if(errno!=ENOENT){
      outMsg(0,'E',"nbMsgLogInitialize: Unable to stat cabal directory %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    if(option>1){
      outMsg(0,'E',"nbMsgLogInitialize: Cabal director %s not found\n",filename);
      return(-1);
      }
    if(msgTrace) outMsg(0,'T',"nbMsgLogInitialize: creating cabal directory %s",filename); 
    if(mkdir(filename,S_IRWXU|S_IRWXG)<0){   // 2012-12-31 eat - VID 605-0.8.13-1 Intentional
      outMsg(0,'E',"nbMsgLogInitialize: Unable to create cabal director %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    }
  else if(!S_ISDIR(filestat.st_mode)){
    outMsg(0,'E',"nbMsgLogInitialize: File %s exists, but is not a directory\n",filename);
    return(-1);
    }
  len=snprintf(filename,sizeof(filename),"message/%s/%s",cabal,nodeName);
  if(len<0 || len>=sizeof(filename)){
    *(filename+sizeof(filename)-1)=0;
    outMsg(0,'E',"nbMsgLogInitialize: File name exceeds buffer size limit - %s",filename);
    return(-1);
    }
  if(lstat(filename,&filestat)<0){  // if cabal node directory not found, create it or find out what's wrong
    if(errno!=ENOENT){
      outMsg(0,'E',"nbMsgLogInitialize: Unable to stat cabal node directory %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    if(option>1){
      outMsg(0,'E',"nbMsgLogInitialize: Cabal node director %s not found\n",filename);
      return(-1);
      }
    if(msgTrace) outMsg(0,'T',"nbMsgLogInitialize: creating cabal directory %s",filename);
    if(mkdir(filename,S_IRWXU|S_IRWXG)<0){   // 2012-12-31 eat - VID 771-0.8.13-1 Intentional
      outMsg(0,'E',"nbMsgLogInitialize: Unable to create cabal node director %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    }
  else if(!S_ISDIR(filestat.st_mode)){
    outMsg(0,'E',"nbMsgLogInitialize: File %s exists, but is not a directory\n",filename);
    return(-1);
    }
  len=snprintf(filename,sizeof(filename),"message/%s/%s/%s.msg",cabal,nodeName,nodeName);
  if(len<0 || len>=sizeof(filename)){
    *(filename+sizeof(filename)-1)=0;
    outMsg(0,'E',"nbMsgLogInitialize: File name exceeds buffer size limit - %s",filename);
    return(-1);
    }
  if(lstat(filename,&filestat)<0){  // look for state file
    if(errno!=ENOENT){
      outMsg(0,'E',"nbMsgLogInitialize: Unable to stat state file %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    if(option>1){
      outMsg(0,'E',"nbMsgLogInitialize: Message log state file %s not found\n",filename);
      return(-1);
      }
    if(option) return(nbMsgFileCreateLinked(context,cabal,nodeName,node)); // create linked 
    else return(nbMsgFileCreateRegular(context,cabal,nodeName,node)); // create regular
    }
  else if(S_ISREG(filestat.st_mode)){
    if(option<3){
      outMsg(0,'E',"Message state file %s already exists as regular file\n",filename);
      return(-1);
      }
    if(option&2) return(nbMsgLogConvert2Linked(context,cabal,nodeName,node));
    if(nbMsgLogEmpty(context,cabal,nodeName)<0) return(-1); // remove node.state file
    return(nbMsgFileCreateLinked(context,cabal,nodeName,node)); // create linked
    }
  else if(S_ISLNK(filestat.st_mode)){
    if(option<2 || option==3){
      outMsg(0,'E',"Message state file %s already exists as symbolic link",filename);
      return(-1);
      }
    if(option&2) return(nbMsgLogConvert2Regular(context,cabal,nodeName,node));
    if(nbMsgLogEmpty(context,cabal,nodeName)<0) return(-1); // remove node.state file
    return(nbMsgFileCreateRegular(context,cabal,nodeName,node)); // create regular
    }
  outMsg(0,'E',"State file %s exists and not a symbolic link or regular file\n",filename);
  return(-1);
  }

/*
*  Prune message log by removing files that only contains messages older than a specified age.
*
*/
int nbMsgLogPrune(nbCELL context,char *cabal,char *nodeName,unsigned char node,int seconds){
  char filename[128];
  struct stat filestat;
  uint32_t tranTime,tranCount,recordTime,recordCount,fileTime,fileCount;
  char fileState;
  char linkname[128];
  char linkedname[32];
  int linklen;
  int file;
  unsigned char buffer[2*1024];
  int len;
  int msglen;
  char *errStr;
  time_t utime;
  time_t pruneTime;
  int deleted=0;          // number of files deleted
  int pruning=0;          // deleting files
  char strTime[64];

  // Get prune time
  time(&utime);
  pruneTime=utime-seconds; 
  strncpy(strTime,ctime(&pruneTime),sizeof(strTime)-1);  // 2012-12-16 eat - CID 751640
  *(strTime+sizeof(strTime)-1)=0;
  outMsg(0,'I',"Pruning cabal %s node %s intstance %d files to %s.",cabal,nodeName,node,strTime);

  // Get the file name
  snprintf(linkname,sizeof(linkname),"message/%s/%s/%s.msg",cabal,nodeName,nodeName); 
  if(lstat(linkname,&filestat)<0){
    outMsg(0,'E',"nbMsgLogPrune: Unable to stat %s - %s",linkname,strerror(errno));
    return(-1);
    }
  if(S_ISLNK(filestat.st_mode)){
    if(msgTrace) outMsg(0,'T',"nbMsgLogOpen: State file %s is a symbolic link",linkname);
    // change this to retry in a loop for a few times if not mode PRODUCER because nbMsgLogFileCreate may be removing and creating link
    linklen=readlink(linkname,linkedname,sizeof(linkedname));
    if(linklen<0){
      outMsg(0,'E',"Unable to read link %s - %s",linkname,strerror(errno));
      return(-1);
      }
    // if(linklen==sizeof(linkname)){  // 2012-12-16 eat - CID 751582
    if(linklen==sizeof(linkedname)){
      outMsg(0,'E',"Symbolic link %s point to file name too long for buffer",linkname);
      return(-1);
      }
    *(linkedname+linklen)=0;
    if(msgTrace) nbLogMsg(context,0,'T',"Cabal %s node %s content link is %s -> %s",cabal,nodeName,linkname,linkedname);
    }
  else if(S_ISREG(filestat.st_mode)){
    outMsg(0,'T',"Expecting %s to be a symbolic link - can not prune state files",linkname);
    return(-1);
    }
  else{
    outMsg(0,'E',"Expecting %s to be a symbolic link or regular file",linkname);
    return(-1);
    }

  // Open and read the bottom file
  sprintf(filename,"message/%s/%s/%s",cabal,nodeName,linkedname);
  if((file=open(filename,O_RDONLY))<0){ 
    outMsg(0,'E',"Unable to open file %s - %s\n",filename,strerror(errno));
    return(-1);
    }
  if((len=read(file,buffer,sizeof(buffer)))<0){
    close(file); // 2012-12-18 eat - CID 751602
    outMsg(0,'E',"Unable to read file %s - %s\n",filename,strerror(errno));
    return(-1);
    }
  msglen=(buffer[0]<<8)|buffer[1];
  if(msglen>len){
    close(file);
    outMsg(0,'E',"Header record of length %d in file %s is larger than allocated buffer length %d.\n",msglen,filename,len);
    return(-1);
    }
  errStr=nbMsgHeaderExtract((nbMsgRec *)buffer,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount,&fileState);
  if(errStr){
    close(file);
    outMsg(0,'E',"Corrupted file %s - %s\n",filename,errStr);
    return(-1);
    }
  close(file);
  // Step up through the files until we find one to prune
  while(!(fileState&NB_MSG_FILE_STATE_FIRST) || pruning){
    outMsg(0,'T',"File %s time %d with %d remaining.",filename,fileTime,fileTime-pruneTime);
    if(fileTime<pruneTime){
      if(pruning){
        if(unlink(filename)<0) outMsg(0,'E',"nbMsgLogPrune: Unable to remove file %s - %s\n",filename,strerror(errno));
        else deleted++;
        } 
      else{             // flag new first file
        int fileStateOffset=sizeof(nbMsgRec)+(((nbMsgRec *)buffer)->msgids+1)*sizeof(nbMsgId);
        if(msglen<=fileStateOffset){       // obsolete file structure
          outMsg(0,'W',"File %s header is obsolete - can not flag as first file - pruning will start later.\n",filename);
          return(-1);
          }
        else{ // record contains fileState
          if((file=open(filename,O_RDWR))<0){ 
            outMsg(0,'E',"Unable to open file %s - %s\n",filename,strerror(errno));
            return(-1);
            }
          if(lseek(file,fileStateOffset,SEEK_SET)<0){
            outMsg(0,'E',"Unable to seek to file state in file %s - %s\n",filename,strerror(errno));
            close(file);  // 2012-12-18 eat - CID 751602
            return(-1);
            }
          fileState|=NB_MSG_FILE_STATE_FIRST;
          if((len=write(file,&fileState,1))<0){
            outMsg(0,'E',"Unable to open file %s - %s\n",filename,strerror(errno));
            close(file);
            return(-1);
            }
          close(file);
          }     
        pruning=1;      // start pruning
        }
      }
    sprintf(filename,"message/%s/%s/%10.10u.msg",cabal,nodeName,fileCount-1);
    if((file=open(filename,O_RDONLY))<0){ 
      outMsg(0,'E',"Unable to open file %s - %s\n",filename,strerror(errno));
      return(-1);
      }
    if((len=read(file,buffer,sizeof(buffer)))<0){
      outMsg(0,'E',"Unable to read file %s - %s\n",filename,strerror(errno));
      close(file);
      return(-1);
      }
    close(file);
    msglen=(buffer[0]<<8)|buffer[1];
    if(msglen>len){
      outMsg(0,'E',"Header record of length %d in file %s is larger than allocated buffer length %d.\n",msglen,filename,len);
      return(-1);
      }
    errStr=nbMsgHeaderExtract((nbMsgRec *)buffer,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount,&fileState);
    if(errStr){
      outMsg(0,'E',"Corrupted file %s - %s\n",filename,errStr);
      return(-1);
      }
    }
  outMsg(0,'I',"Message cabal %s node %s instance %d pruned successfully - %d files deleted.\n",cabal,nodeName,node,deleted);
  return(0);
  }

/*
*  Create a message log file
*
*  Returns:  -1 - error, 0 - success
*/
int nbMsgLogFileCreate(nbCELL context,nbMsgLog *msglog){
  char filebase[32];
  char filename[128];
  char linkname[128];
  char linkedname[512];
  int  node,nodeIndex;
  nbMsgRec *msgrec;
  nbMsgId  *msgid;
  int msgids;
  int msglen;
  int linklen;
  time_t utime;
  int len;

  time(&utime);
  msglog->fileTime=utime;
  msglog->fileCount++;
  if(msglog->file){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: File already open for cabal \"%s\" node %d\n",msglog->cabal,msglog->node);
    return(-1);
    }
  node=msglog->node;
  sprintf(linkname,"message/%s/%s/%s.msg",msglog->cabal,msglog->nodeName,msglog->nodeName);
  linklen=readlink(linkname,linkedname,sizeof(linkedname));
  if(linklen<0){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: Unable to read link %s - %s\n",filename,strerror(errno));
    return(-1);  // 2012-12-18 eat - CID 751564
    }
  if(linklen==sizeof(linkedname)){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: Link %s too long for buffer\n",filename);
    return(-1);
    }
  *(linkedname+linklen)=0;
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogFileCreate: Link is %s -> %s\n",linkname,linkedname);
  if(strcmp(linkedname,"empty")!=0 && strcmp(linkedname,msglog->filename)!=0){
    fprintf(stderr,"nbMsgLogFileCreate: Corrupted link %s -> %s - does not link to active file - %s \\n",linkname,linkedname,msglog->filename);
    return(-1);
    }
  // start a new file
  sprintf(filebase,"%10.10d.msg",msglog->fileCount);
  len=snprintf(filename,sizeof(filename),"message/%s/%s/%s",msglog->cabal,msglog->nodeName,filebase); // 2012-12-31 eat - VID 4608-0.8.13-1
  if(len<0 || len>=sizeof(filename)){
    fprintf(stderr,"nbMsgLogFileCreate: File exceeds buffer limit - %s\n",filename);
    return(-1);
    }
  if((msglog->file=open(filename,O_WRONLY|O_CREAT,S_IRWXU|S_IRGRP))<0){ 
    fprintf(stderr,"nbMsgLogFileCreate: Unable to creat file %s\n",filename);
    return(-1);
    }
  //fprintf(stderr,"nbMsgLogFileCreate: fildes=%d %s\n",msglog->file,filename);
  if(remove(linkname)<0){    // 2012-12-31 eat - VID 762-0.8.13-1 Intentional
    fprintf(stderr,"nbMsgLogFileCreate: Unable to remove symbolic link %s - %s\n",linkname,strerror(errno));
    return(-1);
    }
  if(symlink(filebase,linkname)<0){
    fprintf(stderr,"nbMsgLogFileCreate: Unable to create symbolic link %s - %s\n",linkname,filename);
    return(-1);
    }
  strcpy(msglog->filename,filebase);
  if(!msglog->hdrbuf) msglog->hdrbuf=(nbMsgRec *)nbAlloc(sizeof(nbMsgRec)+256*sizeof(nbMsgId)); // allocate header buffer
  msgrec=msglog->hdrbuf;
  msgrec->type=NB_MSG_REC_TYPE_HEADER;  
  msgrec->datatype=NB_MSG_REC_DATA_ID;  
  msgid=&msgrec->si;
  nbMsgIdStuff(msgid,node,msglog->logState->msgnum[node].time,msglog->logState->msgnum[node].count);
  msgid++;
  nbMsgIdStuff(msgid,node,msglog->recordTime,msglog->recordCount);
  msgid++;
  msgids=0;
  for(nodeIndex=0;nodeIndex<32;nodeIndex++){
    if(nodeIndex!=node && msglog->logState->msgnum[nodeIndex].time!=0){
      msgids++; 
      nbMsgIdStuff(msgid,nodeIndex,msglog->logState->msgnum[nodeIndex].time,msglog->logState->msgnum[nodeIndex].count)
;
      msgid++;
      }
    }
  nbMsgIdStuff(msgid,node,msglog->fileTime,msglog->fileCount);
  msgid++;
  *((char *)msgid)=0;                    // insert fileState
  msgrec->msgids=msgids;
  msglen=(char *)msgid-(char *)msgrec+1; // -1 is for the fileSate
  msgrec->len[0]=msglen>>8;
  msgrec->len[1]=msglen&0xff;
  if(write(msglog->file,msgrec,msglen)<0){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: Unable to write state header to file %s - %s\n",filename,strerror(errno));
    close(msglog->file);
    msglog->file=0;
    return(-1);
    }
  msglog->filesize=msglen;
  return(0);
  }

/*
*  Read incoming packets for message handler
*
*/
void nbMsgProducerUdpRead(nbCELL context,int serverSocket,void *handle){
  nbMsgLog *msglog=(nbMsgLog *)handle;
  char name[32];       // 2012-12-31 eat - VID 4732-0.8.13-1  changed from 33 to 32
  ssize_t  len;
  
  len=recvfrom(msglog->socket,name,sizeof(name),0,NULL,0);
  while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,name,sizeof(name),0,NULL,0);
  if(len<0){
    outMsg(0,'E',"nbMsgProducerUdpRead: Cabal %s node %s ignoring recvfrom len=%d - %s",msglog->cabal,msglog->nodeName,len,strerror(errno));
    return;
    }
  if(len<2){
    outMsg(0,'E',"nbMsgProducerUdpRead: Cabal %s node %s ignoring request with null name",msglog->cabal,msglog->nodeName);
    return;
    }
  if(name[len-1]!=0){
    outMsg(0,'E',"nbMsgProducerUdpRead: Cabal %s node %s ignoring request with out null terminator",msglog->cabal,msglog->nodeName);
    return;
    }
  *(name+sizeof(name)-1)=0;  // 2013-01-04 eat - CID 751628 FP see if this convinces the checker - seems to miss that we checked above
  if(*(name+sizeof(name)-1)!=0){ // 2013-01-04 eat - hey let's get really crazy until the checker agrees with me
    outMsg(0,'E',"nbMsgProducerUdpRead: Cabal %s node %s ignoring request with out null terminator",msglog->cabal,msglog->nodeName);
    return;
    }
  nbMsgConsumerAdd(msglog,name);
  }

/*
*  Switch message log from reading to writing (producer) mode
*
*  Returns: -1 - error, 0 - success
*/
int nbMsgLogProduce(nbCELL context,nbMsgLog *msglog,uint32_t maxfilesize){
  char filename[256];
  unsigned char  node;
  DIR *dir;
  struct dirent *ent;
  int fd;
  int len;

  //fprintf(stderr,"nbMsgLogProduce: called with maxfilesize=%u\n",maxfilesize);
  node=msglog->node;
  if(!(msglog->state&NB_MSG_STATE_LOGEND)){
    outMsg(0,'E',"nbMsgLogProduce: Message log not in end-of-log state - cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(-1);
    }
  len=snprintf(filename,sizeof(filename),"message/%s/%s/%s.msg",msglog->cabal,msglog->nodeName,msglog->nodeName);
  if(len<0 || len>=sizeof(filename)){
    *(filename+sizeof(filename)-1)=0;
    outMsg(0,'E',"nbMsgLogProduce: Filename exceeds buffer limit - %s",filename);
    return(-1);
    }
  if(msglog->file){
    outMsg(0,'E',"nbMsgLogProduce: File open - expecting closed file");
    return(-1);
    }
  msglog->maxfilesize=maxfilesize;
  msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  if(msglog->option&NB_MSG_OPTION_STATE){
    if((msglog->file=open(filename,O_WRONLY))<0){
      outMsg(0,'E',"nbMsgLogProduce: Unable to append to file %s",filename);
      return(-1);
      }
    }
  else{    // Prepare to communicate with consumers if we aren't just maintaining state---actually logging messages
    if((msglog->file=open(filename,O_WRONLY|O_APPEND,S_IRWXU|S_IRGRP))<0){ 
      outMsg(0,'E',"nbMsgLogProduce: Unable to append to file %s",filename);
      return(-1);
      }
    len=snprintf(filename,sizeof(filename),"message/%s/%s/~.socket",msglog->cabal,msglog->nodeName);
    if(len<0 || len>=sizeof(filename)){
      *(filename+sizeof(filename)-1)=0;
      outMsg(0,'E',"nbMsgLogProduce: Filename exceeds buffer limit - %s",filename);
      return(-1);
      }
    fd=nbIpGetUdpServerSocket(context,filename,0);
    if(fd<0){
      outMsg(0,'E',"nbMsgLogProduce: Unable to open udp server socket %s",filename);
      return(-1);
      }
    msglog->socket=fd;
    //outMsg(0,'T',"nbMsgLogProduce: set fd=%d to non-blocking",msglog->socket);
    if(fcntl(msglog->socket,F_SETFL,fcntl(msglog->socket,F_GETFL)|O_NONBLOCK)){ // make it non-blocking // 2012-12-27 eat 0.8.13 - CID 761521
      outMsg(0,'E',"nbMsgLogProduce: Unable to make udp server socket %s non-blocking - %s",filename,strerror(errno));
      return(-1);
      }
    //outMsg(0,'T',"nbMsgLogProduce: F_GETFL return=%d",fcntl(msglog->socket,F_GETFL));
    nbListenerAdd(context,msglog->socket,msglog,nbMsgProducerUdpRead);
    // 2011-01-19 eat - finishing up support for multiple consumers
    // Read the directory and call nbMsgConsumerAdd for every *.socket other than ~.socket
    // Entries will automatically get deleted if the consumers are not listening
    len=snprintf(filename,sizeof(filename),"message/%s/%s",msglog->cabal,msglog->nodeName);
    if(len<0 || len>=sizeof(filename)){
      *(filename+sizeof(filename)-1)=0;
      outMsg(0,'E',"nbMsgLogProduce: Filename exceeds buffer limit - %s",filename);
      return(-1);
      }
    dir=opendir(filename);
    if(!dir){
      outMsg(0,'E',"nbMsgLogProduce: Unable to open directory %s",filename);
      return(-1);
      }
    while((ent=readdir(dir))){  // for each entry
      int len=strlen(ent->d_name);
      //char name[32],*delim;
      char name[32];
      //outMsg(0,'T',"nbMsgLogProduce: file=%s/%s",filename,ent->d_name);
      if(len>7 && strcmp(ent->d_name+len-7,".socket")==0 && strcmp(ent->d_name,"~.socket")!=0){
        len-=7; // 2012-12-31 eat - VID 643-0.8.13-1 
        //delim=strchr(ent->d_name,'.');
        //len=delim-ent->d_name;
        if(len<sizeof(name)){
          strncpy(name,ent->d_name,len);
          *(name+len)=0; 
          nbMsgConsumerAdd(msglog,name);
          }
        else outMsg(0,'E',"nbMsgLogProduce: consumer filename %s greater than %d characters - ignored",ent->d_name,sizeof(name)-1);
        }
      }
    closedir(dir);
    }
  return(0);
  }

/*
*  Write message to log
*
*    This function is called to write a message to an active message log file and
*    and the corresponding local domain socket via UDP.
*    
*  Returns: -1 error, 0 - success, 1 - unable to write to UDP socket
*/
int nbMsgLogWrite(nbCELL context,nbMsgLog *msglog,uint16_t msglen){   // 2012-12-31 eat - VID 4842,5479-0.8.13-1 from int to uint16_t
  time_t utime;
  int node=msglog->node;
  nbMsgCursor *msgudp=(nbMsgCursor *)msglog->msgbuf;
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));
  nbMsgRec *hdrrec;
  nbMsgId *msgid;
  int footerfile;
  int footerlen;
  int i;
  struct timeval tv;

  msgrec->len[0]=msglen>>8;    // save lenth in network byte order
  msgrec->len[1]=msglen%256;   // 2012-12-31 eat - VID 4842,5479-0.8.13-1
  msglog->filesize+=msglen;    // 2012-12-31 eat - VID 5045,5418-0.8.13-1
  msgudp->fileCount=msglog->fileCount;
  msgudp->fileOffset=msglog->filesize;
  // *** need to include sizeof(nbMsgId)*nodes in state vector
  // although this isn't critical because the file size can go over the max a bit
  if(msglog->filesize>msglog->maxfilesize-sizeof(nbMsgRec)){  // start a new file when we hit the max size
    footerfile=msglog->file;
    msglog->file=0;              // clear before letting nbMsgLogFileCreate reuse
    // create new file
    if(nbMsgLogFileCreate(context,msglog)!=0){
      nbLogMsg(context,0,'E',"nbMsgLogWrite: Unable to create new message file for cabal \"%s\" node %d",msglog->cabal,msglog->node);
      close(footerfile);
      return(-1);
      }
    // write footer - just another copy of the header on the next file just created
    hdrrec=msglog->hdrbuf; // allocated by nbMsgLogFileCreate
    hdrrec->type=NB_MSG_REC_TYPE_FOOTER;
    footerlen=(msglog->hdrbuf->len[0]<<8)|msglog->hdrbuf->len[1];
    if(write(footerfile,msglog->hdrbuf,footerlen)<0){
      nbLogMsg(context,0,'E',"nbMsgLogWrite: Unable to write cabal \"%s\" node %d fildes %d - %s",msglog->cabal,msglog->node,footerfile,strerror(errno));
      close(footerfile);
      msglog->file=0;
      return(-1);
      }
    close(footerfile);
    msglog->filesize+=msglen; // need to add length again after new file creation
    msgudp->fileCount=msglog->fileCount; // use new file info
    msgudp->fileOffset=msglog->filesize;
    }
  // write a message
  msgid=&msgrec->pi;
  time(&utime);
  msglog->recordCount++;  // let it wrap - zero is not a special value here
  nbMsgIdStuff(msgid,node,utime,msglog->recordCount);
  // write to message log file
  gettimeofday(&tv,NULL); // get seconds an microseconds
  if(msgTrace){
    nbLogMsg(context,0,'T',"nbMsgLogWrite: writing message %10.10lu.%6.6u",tv.tv_sec,tv.tv_usec);
    nbMsgPrint(stderr,msgrec);
    }
  // Let's make sure it is well formed first - at least for now
  if(msgrec->type!=NB_MSG_REC_TYPE_MESSAGE){
    nbLogMsg(context,0,'L',"nbMsgLogWrite: Bad message record type - probably via WriteReplica - terminating");
    exit(1);
    }
  if(sizeof(msgrec)+msgrec->msgids*sizeof(msgrec)>msglen){
    nbLogMsg(context,0,'L',"nbMsgLogWrite: Bad msgids field - probably via WriteReplica - terminating");
    exit(1);
    }  
  msgid=&msgrec->si;
  for(i=msgrec->msgids+2;i;i--){
    if(msgid->node>10){  // while testing with only two nodes
      nbLogMsg(context,0,'L',"nbMsgLogWrite: Bad msgids field - probably via WriteReplica - terminating");
      nbLogFlush(context);
      exit(1);
      }
    }
  if(msglog->file && write(msglog->file,msgrec,msglen)<0){
    nbLogMsg(context,0,'E',"nbMsgLogWrite: Unable to write cabal \"%s\" node %d fildes %d - %s",msglog->cabal,msglog->node,msglog->file,strerror(errno));
    close(msglog->file);
    msglog->file=0;
    return(-1);
    }
  // Send message to registered consumers via UDP sockets
  if(msglog->socket) nbMsgConsumerSend(context,msglog,msgudp,msglen+sizeof(nbMsgCursor)); // don't care what happens
  return(0);
  }

/*
*  Write string message
*/
int nbMsgLogWriteString(nbCELL context,nbMsgLog *msglog,unsigned char *text){
  unsigned short msglen;
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));
  unsigned short datalen=strlen((char *)text)+1; // include null char delimiter
  nbMsgId *msgid=&msgrec->si;
  time_t utime;
  int node=msglog->node;
  size_t n,i;

  if(datalen>NB_MSG_REC_MAX-sizeof(nbMsgRec)){
    nbLogMsg(context,0,'E',"nbMsgLogWriteOriginal: Data length %u exceeds max of %u",datalen,NB_MSG_REC_MAX-sizeof(nbMsgRec));
    return(-1);
    }
//2012-01-31 dtl: msgbuf has sizeof NB_MSG_BUF_LEN, safe to copy after tested
// memcpy(msglog->msgbuf+sizeof(nbMsgCursor)+sizeof(nbMsgRec),text,datalen);
  n=sizeof(nbMsgCursor)+sizeof(nbMsgRec); //calculate offset
  if((n+datalen)<=NB_MSG_BUF_LEN) for(i=0;i<datalen;i++) *(msglog->msgbuf+n+i)=*(text+i); //dtl: replaced memcpy
  msglen=sizeof(nbMsgRec)+datalen;
  msgrec->type=NB_MSG_REC_TYPE_MESSAGE;
  msgrec->datatype=NB_MSG_REC_DATA_CHAR;
  msgrec->msgids=0;
  msgid=&msgrec->si;
  time(&utime);
  if(utime<msglog->logState->msgnum[msglog->node].time){
    nbLogMsg(context,0,'E',"nbMsgLogWrite: Log state for cabal \"%s\" node %d is in the future - %d at %d\n",msglog->cabal,msglog->node,msglog->logState->msgnum[msglog->node].time,utime);
    exit(1);
    }
  else msglog->logState->msgnum[node].time=utime;
  msglog->logState->msgnum[node].count++;
  if(msglog->logState->msgnum[node].count==0) msglog->logState->msgnum[node].count++; // skip special value of zero
  nbMsgIdStuff(msgid,node,msglog->logState->msgnum[node].time,msglog->logState->msgnum[node].count);
  return(nbMsgLogWrite(context,msglog,msglen));
  }

/*
*  Write data message (may be text or binary data)
   datalen = sizeof(data) */
int nbMsgLogWriteData(nbCELL context,nbMsgLog *msglog,void *data,unsigned short datalen){
  unsigned short msglen;
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));
  nbMsgId *msgid=&msgrec->si;
  time_t utime;
  int node=msglog->node;
  int n,i;
 
  if(datalen>NB_MSG_REC_MAX-sizeof(nbMsgRec)){
    nbLogMsg(context,0,'E',"nbMsgLogWriteOriginal: Data length %u exceeds max of %u",datalen,NB_MSG_REC_MAX-sizeof(nbMsgRec)); 
    return(-1);
    }
//2012-01-31 dtl: msgbuf has sizeof NB_MSG_BUF_LEN, safe to copy after tested
//memcpy(msglog->msgbuf+sizeof(nbMsgCursor)+sizeof(nbMsgRec),data,datalen);
  n=sizeof(nbMsgCursor)+sizeof(nbMsgRec); //offset
  if((n+datalen)<=NB_MSG_BUF_LEN) for(i=0;i<datalen;i++) *(msglog->msgbuf+n+i)=*((unsigned char*)data +i); //dtl:replaced memcpy
  msglen=sizeof(nbMsgRec)+datalen; 
  msgrec->type=NB_MSG_REC_TYPE_MESSAGE;
  msgrec->datatype=NB_MSG_REC_DATA_BIN;
  msgrec->msgids=0;
  msgid=&msgrec->si;
  time(&utime);
  if(utime<msglog->logState->msgnum[msglog->node].time){
    nbLogMsg(context,0,'E',"nbMsgLogWrite: Log state for cabal \"%s\" node %d is in the future - %d at %d",msglog->cabal,msglog->node,msglog->logState->msgnum[msglog->node].time,utime);
    exit(1);
    }
  else msglog->logState->msgnum[node].time=utime;
  msglog->logState->msgnum[node].count++;
  if(msglog->logState->msgnum[node].count==0) msglog->logState->msgnum[node].count++; // skip special value of zero
  nbMsgIdStuff(msgid,node,msglog->logState->msgnum[node].time,msglog->logState->msgnum[node].count);
  return(nbMsgLogWrite(context,msglog,msglen));
  }

/*
*  Write message replica to log
*
*  Returns:
*
*    -1 - syntax error in msg
*     0 - not new to pgm
*     1 - msg text is new to pgm 
*/
int nbMsgLogWriteReplica(nbCELL context,nbMsgLog *msglog,nbMsgRec *msgin){
  int state;
  unsigned short int msglen;
  //off_t pos;
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));

  if(msgTrace){
    nbLogMsg(context,0,'T',"nbMsgLogWriteReplica: called");
    nbMsgPrint(stderr,msgin);
    }
  state=nbMsgLogSetState(context,msglog,msgin);
  //nbLogMsg(context,0,'T',"nbMsgLogWriteReplica: nbMsgLogSetState returned state=%d",state);
  if(state<0){
    nbLogMsg(context,0,'E',"nbMsgLogWriteReplica: Format error in cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(-1);
    }
  if(state&NB_MSG_STATE_LOG){
    if(msglog->option&NB_MSG_OPTION_CONTENT){
      msglen=CNTOHS(((unsigned char *)&msgin->len));
      if(msglen<sizeof(nbMsgRec)){
        nbLogMsg(context,0,'E',"nbMsgLogWriteReplica: Message length %u less than min %u",msglen,sizeof(nbMsgRec));
        return(-1);
        }
      memcpy(msgrec,msgin,sizeof(nbMsgRec)-sizeof(nbMsgId));  // copy header - less transaction msgid
      msgrec->msgids=msgin->msgids+1;
      msglen+=sizeof(nbMsgId);
      memcpy(((char *)msgrec)+sizeof(nbMsgRec),&msgin->pi,msglen-sizeof(nbMsgRec));
      //nbLogMsg(context,0,'T',"nbMsgLogWriteReplica: writing to log");
      nbMsgLogWrite(context,msglog,msglen);  // this function will fill in the first msgid
      }
    else if(msglog->option&NB_MSG_OPTION_STATE){
      lseek(msglog->file,0,SEEK_SET);
      if(nbMsgFileWriteState(context,msglog->file,msglog->logState,msglog->node,0,0,0,NB_MSG_FILE_STATE_ONLY)<0){
        nbLogMsg(context,0,'E',"nbMsgLogWriteReplica: Cabal %s node %s - write of state header failed - %s",msglog->cabal,msglog->nodeName,strerror(errno));
        return(-1);
        }
      }
    }
  if(state&NB_MSG_STATE_PROCESS) return(1);
  return(0);
  }

//==============================================================================
// Message Cache API
//==============================================================================

/*
*  Fill subscriber's buffer from msglog
*
*  Returns: Number of messages copied to buffer
*/
int nbMsgCachePublish(nbCELL context,nbMsgCacheSubscriber *msgsub){
  nbMsgCache *msgcache=msgsub->msgcache;
  nbMsgRec *msgrec;
  int state=0;
  int messages=0;
  unsigned char *cachePtr;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCachePublish: called with flags=%2.2x",msgsub->flags);
  //if(msgsub->flags&NB_MSG_CACHE_FLAG_MSGLOG){
  // 2010-05-06 eat - msglog could have hit end of file, so we need to check FLAG_INBUF without first checking FLAG_MSGLOG
  if(msgsub->flags&NB_MSG_CACHE_FLAG_INBUF){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCachePublish: calling subscription handler for messaging remaining in message log buffer");
    if((*msgsub->handler)(context,msgsub->handle,msgsub->msglog->msgrec)) return(0);
    else{
      nbLogMsg(context,0,'T',"nbMsgCachePublish: turning FLAG_INBUF off");
      msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_INBUF;
      messages++;
      }
    }
  msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_PAUSE;  // turn off pause flag - subscriber may have called us
  if(msgsub->flags&NB_MSG_CACHE_FLAG_MSGLOG){
    nbLogMsg(context,0,'T',"nbMsgCachePublish: msgsub=%p msgsub->msglog=%p",msgsub,msgsub->msglog);
    while(!(state&NB_MSG_STATE_LOGEND)){
      state=nbMsgLogRead(context,msgsub->msglog);
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCachePublish: nbMsgLogRead returned state=0x%u",state);
      if(state&NB_MSG_STATE_PROCESS){
        if(msgTrace) nbMsgPrint(stderr,msgsub->msglog->msgrec);
        if((*msgsub->handler)(context,msgsub->handle,msgsub->msglog->msgrec)){
          nbLogMsg(context,0,'T',"nbMsgCachePublish: turning FLAG_INBUF and FLAG_PAUSE on");
          msgsub->flags|=NB_MSG_CACHE_FLAG_INBUF|NB_MSG_CACHE_FLAG_PAUSE;
          return(messages);
          }
        else messages++; 
        }
      }
    nbLogMsg(context,0,'T',"nbMsgCachePublish: End of log");
    // let's start reading from the cache until we get invalidated again
    // This is an over simplification for prototyping.  We should watch
    // above to see when we can switch to the cache at the first opportunity.
    // That will require a new function to close the message log file without
    // fully closing the msglog structure.
    // When nbMsgCacheInsert finds that it has to stomp on our msgsub->cachePtr
    // it will switch us back to msglog mode.
    msgsub->cachePtr=msgcache->start;
    msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_MSGLOG;
    // we drop through to spin forward in the cache to sync up
    }
  cachePtr=msgsub->cachePtr;
  while(cachePtr!=msgcache->end){
    // make this a switch instead
    if(*cachePtr==0xff) cachePtr=msgcache->bufferStart; // wrap on stop if not at end
    else if(*cachePtr==0x80) cachePtr+=sizeof(nbMsgCacheFileMarker);
    // we can ignore file markers here because nbMsgCacheInsert will
    // adjust the file position in the msglog if it has to switch this
    // subscriber back to msglog mode
    else if(*cachePtr==0){
      msgrec=(nbMsgRec *)(cachePtr+1);
      if(msgTrace) nbMsgPrint(stderr,msgrec);
      if(msgrec->type!=NB_MSG_REC_TYPE_MESSAGE){
        nbLogMsg(context,0,'L',"Fatal error in message cache - invalid message record type %u - terminating",msgrec->type);
        nbLogFlush(context);
        exit(1);
        }
      if(msgsub->flags&NB_MSG_CACHE_FLAG_AGAIN){
        state=NB_MSG_STATE_PROCESS;
        msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_AGAIN;
        }
      else state=nbMsgLogSetState(context,msgsub->msglog,msgrec);
      if(state&NB_MSG_STATE_PROCESS){
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCachePublish: calling handler");
        if((*msgsub->handler)(context,msgsub->handle,msgrec)){
          // 2010-05-06 eat - doesn't seem like we should set INBUF here
          //msgsub->flags|=NB_MSG_CACHE_FLAG_INBUF|NB_MSG_CACHE_FLAG_PAUSE;
          msgsub->flags|=NB_MSG_CACHE_FLAG_PAUSE|NB_MSG_CACHE_FLAG_AGAIN;
          msgsub->cachePtr=cachePtr;
          return(messages);
          }
        else messages++;
        }
      else if(state&NB_MSG_STATE_SEQHIGH){
        nbLogMsg(context,0,'L',"nbMsgCachePublish: Record count sequence error - terminating");
        nbLogFlush(context);
        exit(1);
        }
      cachePtr++; // step over flag byte
      cachePtr+=(*cachePtr<<8)|*(cachePtr+1); 
      }
    else{
      nbLogMsg(context,0,'L',"Fatal error in message cache - invalid entry type %x - terminating",*cachePtr);
      nbLogFlush(context);
      exit(1);
      }
    }
  msgsub->cachePtr=cachePtr;
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCachePublish: Messages=%d",messages);
  return(messages);
  }

/*
*  Register message cache subscriber
*/
nbMsgCacheSubscriber *nbMsgCacheSubscribe(nbCELL context,nbMsgCache *msgcache,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
  nbMsgCacheSubscriber *msgsub;
  nbMsgLog *msglog;
  char *cabalName;
  char *nodeName;
  int node;

  msgsub=(nbMsgCacheSubscriber *)nbAlloc(sizeof(nbMsgCacheSubscriber));
  memset(msgsub,0,sizeof(nbMsgCacheSubscriber));
  msgsub->next=msgcache->msgsub;
  msgsub->msgcache=msgcache;
  msgsub->flags=NB_MSG_CACHE_FLAG_MSGLOG;
  msgsub->handle=handle;
  msgsub->handler=handler;
  msgcache->msgsub=msgsub;

  // for now let's just read from log to get started
  //   This will be enhanced to first check to see if the subscriber's state is
  //   greater than the starting state of the cache.  If so we would prefer to
  //   begin consumer from the cache instead of opening and reading the message
  //   log file.  This requires a "soft open" of the message log where the structure
  //   is properly initialized without actually reading the file.  This will improve
  //   performance in cases where processes are being bounced---connections are
  //   dropped and rebuilt in a relatively short period of time relative to the
  //   cache size and message rate.
  cabalName=msgcache->msglog->cabal;
  nodeName=msgcache->msglog->nodeName;
  node=msgcache->msglog->node;
  msglog=nbMsgLogOpen(context,cabalName,nodeName,node,"",NB_MSG_MODE_CONSUMER,msgstate);
  if(!msglog){
    nbLogMsg(context,0,'E',"nbMsgCacheSubscribe: Unable to open cabal \"%s\" node %s message log",cabalName,nodeName);
    nbMsgCacheCancel(context,msgsub);
    return(NULL);
    }
  msgsub->msglog=msglog;
  return(msgsub); 
  }

/*
*  Remove a message cache subscriber
*
*  Returns: 0 - success, 1 - not found
*/
int nbMsgCacheCancel(nbCELL context,nbMsgCacheSubscriber *msgsub){
  nbMsgCache *msgcache=msgsub->msgcache;
  nbMsgCacheSubscriber **msgsubP;

  for(msgsubP=&msgcache->msgsub;*msgsubP && *msgsubP!=msgsub;msgsubP=&((*msgsubP)->next));
  if(!*msgsubP) return(1);
  *msgsubP=msgsub->next;
  nbFree(msgsub,sizeof(nbMsgCacheSubscriber));
  return(0); 
  }

/*
*  Stomp on messages in queue if necessary to create room for new entry
*
*    This function determines if any messages not yet used by a subscriber will be overwritten
*    by a new message or log file jump marker.  If so, the subscriber(s) are switched to a mode
*    where they read the message log independently until they can use the message cache again.
*/
unsigned char *nbMsgCacheStomp(nbCELL context,nbMsgCache *msgcache,int msglen){
  nbMsgCacheSubscriber *msgsub;
  unsigned char *msgqrec,*msgqstop;
  int qmsglen;
  int looking=1;

  msgqrec=msgcache->end;   // start at end
  msgqstop=msgqrec+1+msglen;  // where new entry would stop
  while(1){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: Looking in cache bufS=%p bufE=%p s=%p e=%p r=%p stop=%p",msgcache->bufferStart,msgcache->bufferEnd,msgcache->start,msgcache->end,msgqrec,msgqstop);
    if(msgqrec>msgcache->start){ // watch out for the buffer end
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: end>start");
      if(msgqstop<msgcache->bufferEnd) return(msgqrec); // we have room
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: stop>=bufEnd");
      msgqrec=msgcache->bufferStart;
      msgqstop=msgqrec+1+msglen;
      }
    else if(msgqstop>=msgcache->start){  // we are in front of start and going to overlay it
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: stop>=start");
      if(*msgcache->start==0x80){   // handle file marker - update file count and offset to jump to next file
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: Found file marker cache.start=%p cache.fileCount=%u cache.fileOffset=%u",msgcache->start,msgcache->fileCount,msgcache->fileOffset);
    
        msgcache->fileCount++;
        //msgcache->fileOffset=*(uint32_t *)(msgcache->start+1);
        msgcache->fileOffset=CNTOHL(msgcache->start+1);
        msgcache->start+=sizeof(nbMsgCacheFileMarker);
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: After file marker cache.start=%p cache.fileCount=%u cache.fileOffset=%u",msgcache->start,msgcache->fileCount,msgcache->fileOffset);
        }
      else if(*msgcache->start!=0xff){
        if(msgTrace){
          nbLogMsg(context,0,'T',"nbMsgCacheStomp: Have to make room for new record - *start=%2.2x",*msgcache->start);
          nbMsgPrint(stderr,(nbMsgRec *)(msgcache->start+1));
          }
        // before stomping on the start record we need to see if any subscribers
        // will need to switch to msglog mode
        // if we save stomp=msgcache->start this can be handled in the spin below
        for(msgsub=msgcache->msgsub;msgsub;msgsub=msgsub->next){
          if(msgsub->cachePtr==msgcache->start && !(msgsub->flags&NB_MSG_CACHE_FLAG_MSGLOG)){
            if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: Switching msgsub=%p msglog=%p to file mode",msgsub,msgsub->msglog);
            msgsub->flags|=NB_MSG_CACHE_FLAG_MSGLOG;
            msgsub->cachePtr=NULL;
            msgsub->msglog->fileCount=msgcache->fileCount;    // position message log for reading
            msgsub->msglog->fileOffset=msgcache->fileOffset;  // 2011-01-03 eat
            msgsub->msglog->filesize=msgcache->fileOffset;
            if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheStomp: Switched msgsub=%p msglog=%p fileCount=%u fileOffset=%u filesize=%u",msgsub,msgsub->msglog,msgsub->msglog->fileCount,msgsub->msglog->fileOffset,msgsub->msglog->filesize);
            }
          }
        // set state of cache start
        if(nbMsgStateSetFromMsgId(context,msgcache->startState,&((nbMsgRec *)(msgcache->start+1))->si)<0){
          nbLogMsg(context,0,'E',"nbMsgCacheStomp: Sequence error at start of message cache");
          return(NULL);
          }
        qmsglen=(*(msgcache->start+1)<<8)+*(msgcache->start+2); // get length of message we are stomping on
        msgcache->fileOffset+=qmsglen;  // maintain offset of queue start in message log file
        msgcache->start+=1+qmsglen; // step to next record
        if(msgTrace) nbLogMsg(context,0,'E',"nbMsgCacheStomp: msgcache->start=%p",msgcache->start);
        if(*msgcache->start==0xff){
          if(msgcache->start==msgcache->end){
            msgqrec=msgcache->bufferStart;
            msgqstop=msgqrec+1+msglen;
            if(msgqstop>=msgcache->bufferEnd){
              nbLogMsg(context,0,'E',"nbMsgCacheStomp: Message too large for message cache buffer - make cache size 256KB or more");
              return(NULL);
              }
            looking=0;
            }
          msgcache->start=msgcache->bufferStart;
          }
        }
      else return(msgqrec);
      }
    else return(msgqrec);
    }
  }

/*
*  Place a file marker in the cache buffer to track position across log files
*/
void nbMsgCacheMarkFileJump(nbCELL context,void *handle,uint32_t fileOffset){
  nbMsgCache *msgcache=handle;
  unsigned char *msgqrec;

  nbLogMsg(context,0,'T',"nbMsgCacheMarkFileJump: call with offset=%u",fileOffset);
  msgqrec=nbMsgCacheStomp(context,msgcache,sizeof(nbMsgCacheFileMarker)); // where do we put it?
  *msgqrec=0x80; msgqrec++;
  *msgqrec=fileOffset>>24; msgqrec++;
  *msgqrec=(fileOffset>>16)&0xff; msgqrec++;
  *msgqrec=(fileOffset>>8)&0xff; msgqrec++;
  *msgqrec=fileOffset&0xff; msgqrec++;
  *msgqrec=0xff;
  msgcache->end=msgqrec;
  }

/*
*  Insert message in cache
*
*    This function is a msglog message handler (consumer), so the parameters
*    are fixed by the Message Log API.
*
*  Returns:
*     -1 - State message sequence error
*      0 - ok
*      1 - File message sequence error
*
*/
int nbMsgCacheInsert(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbMsgCache *msgcache=handle;
  nbMsgCacheSubscriber *msgsub;
  unsigned char *msgqrec,*msgqstop;
  nbMsgId *msgid;
  int msglen;
  uint32_t recordCount;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheInsert: buffer start=%2.2x%2.2x%2.2x",*msgcache->bufferStart,*(msgcache->bufferStart+1),*(msgcache->bufferStart+2));
  msglen=(msgrec->len[0]<<8)|msgrec->len[1];
  msgid=&msgrec->pi;
  recordCount=CNTOHL(msgid->count);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheInsert: msglen=%d recordCount=%u msgcache->endCount=%u",msglen,recordCount,msgcache->endCount);
  if(recordCount!=msgcache->endCount+1){
    nbLogMsg(context,0,'T',"nbMsgCacheInsert: out of sequence code - recordCount=%u endCount=%u - terminating",recordCount,msgcache->endCount);
    exit(1);
    }
  msgcache->endCount=recordCount;
  if(msgTrace){ // (msgTrace) info
    msgid=&msgrec->si;
    recordCount=CNTOHL(msgid->count);
    nbLogMsg(context,0,'T',"nbMsgCacheInsert: tranCount=%u",recordCount);
    }
  // check to see if we already have this message - UDP may be behind file
  if(nbMsgStateCheck(msgcache->endState,&msgrec->si)<0){
    if(msgTrace) nbLogMsg(context,0,'I',"nbMsgCacheInsert: Ignoring message we've seen before\n");
    return(0);
    }
  msgqrec=nbMsgCacheStomp(context,msgcache,msglen);  // stomp a place into the cache
  *msgqrec=0;
  memcpy(msgqrec+1,msgrec,msglen);
  msgqstop=msgqrec+1+msglen;
  *msgqstop=0xff;     // flag new end of cache
  msgcache->end=msgqstop;
  if(msgTrace) nbLogMsg(context,0,'T',"Found   in cache area bufS=%p bufE=%p s=%p e=%p r=%p stop=%p",msgcache->bufferStart,msgcache->bufferEnd,msgcache->start,msgcache->end,msgqrec,msgqstop);
  //nbMsgPrint(stderr,msgrec);  // debug
  //if(msgrec->type==NB_MSG_REC_TYPE_MESSAGE && msgrec->datatype==NB_MSG_REC_DATA_CHAR){
  //  char *cmd=((char *)msgrec)+sizeof(nbMsgRec)+msgrec->msgids*sizeof(nbMsgId);
  //  nbCmd(context,cmd,1);       // performance testing
  //  }
  
  // spin through subscribers and notify anyone that needs notification
  for(msgsub=msgcache->msgsub;msgsub;msgsub=msgsub->next){
    if(!(msgsub->flags&(NB_MSG_CACHE_FLAG_PAUSE|NB_MSG_CACHE_FLAG_MSGLOG))){
      if(nbMsgCachePublish(context,msgsub)) msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_PAUSE; 
      }
    }
  return(0);
  }


/*
*  Free up a message cache structure
*/
void nbMsgCacheFree(nbCELL context,nbMsgCache *msgcache){
  if(msgcache->bufferStart) nbFree(msgcache->bufferStart,msgcache->bufferSize);
  if(msgcache->startState) nbMsgStateFree(context,msgcache->startState);
  if(msgcache->endState) nbMsgStateFree(context,msgcache->endState);
  nbFree(msgcache,sizeof(nbMsgCache));
  }

/*
*  Open a message cache and listen for UDP packets
*/ 
nbMsgCache *nbMsgCacheAlloc(nbCELL context,char *cabal,char *nodeName,unsigned char node,size_t size){
  nbMsgCache *msgcache;
  nbMsgLog *msglog;
  unsigned char *msgqrec;

  msgcache=(nbMsgCache *)nbAlloc(sizeof(nbMsgCache));
  memset(msgcache,0,sizeof(nbMsgCache));
  msgcache->bufferSize=size;
  msgcache->bufferStart=nbAlloc(size);
  msgcache->bufferEnd=msgcache->bufferStart+size;
  msgqrec=msgcache->bufferStart;
  *msgqrec=0xff;  // when first flag byte is 0xff we are at the end of the cache or end of cache buffer
  msgcache->start=msgqrec;
  msgcache->end=msgqrec;
  msgcache->startState=nbMsgStateCreate(context);
  msgcache->endState=nbMsgStateCreate(context);

  msglog=nbMsgLogOpen(context,cabal,nodeName,node,"",NB_MSG_MODE_CONSUMER|NB_MSG_MODE_LASTFILE,msgcache->endState);
  if(!msglog){
    nbLogMsg(context,0,'E',"nbMsgCacheOpen: Unable to open message log for cabal \"%s\" node %d",cabal,node);
    nbMsgCacheFree(context,msgcache);
    return(NULL);
    }
  msglog->fileJumper=nbMsgCacheMarkFileJump; // provide method to create file markers in the message cache
  msgcache->msglog=msglog;
  msgcache->endCount=msglog->recordCount;
  msgcache->fileCount=msglog->fileCount;   // initialize file count and offset
  msgcache->fileOffset=msglog->filesize;
  if(nbMsgLogConsume(context,msglog,msgcache,nbMsgCacheInsert)!=0){
    nbMsgCacheFree(context,msgcache);
    return(NULL);
    }
  return(msgcache);
  }

//==============================================================================
// Message Cabal API
//==============================================================================

//***************************************************************************
// NodeBrain Asynchronous Peer API Routines
//***************************************************************************
 
/*
*  Connection shutdown handler
*/
static void nbMsgPeerShutdown(nbCELL context,nbPeer *peer,void *handle,int code){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  time_t utime;
  nbLogMsg(context,0,'T',"nbMsgPeerShutdown: Cabal %s node %s connection %s is shutting down",msgnode->msgcabal->cabalName,msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
  time(&utime);             // get utc time
  msgnode->downTime=utime;  // time stamp the disconnect
  //nbMsgCabalEnable(context,msgnode->msgcabal);
  // 2011-02-19 eat 0.8.5 - set or reset the timer on the cabal
  nbSynapseSetTimer(context,msgnode->msgcabal->synapse,30);
  }

/*
*  Standard peer API producer once connection is established
*
*    This routine should be used in situations where you don't want to 
*    produce anything, but can't pass a null producer to the nbPeerModify
*    because there may be something in the buffer still to write.  This
*    producer allows the buffer to be written and then the listener will
*    stop after getting nothing from this producer.
*
*    NOTE: We need to work out a similar relationship on the consumer side.
*/
static int nbMsgPeerNullProducer(nbCELL context,nbPeer *peer,void *handle){
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerNullProducer: called - returning nothing");
  return(0);
  }

/*
*  Listener's connection shutdown handler - before node is known
*/
static void nbMsgPeerAcceptShutdown(nbCELL context,nbPeer *peer,void *handle,int code){
  nbMsgCabal *msgcabal=(nbMsgCabal *)handle;
  nbLogMsg(context,0,'T',"nbMsgPeerAcceptShutdown: Cabal %s connection %s is shutting down before node identification",msgcabal->cabalName,peer->tls->uriMap[peer->tls->uriIndex].uri);
  // That's ok, this wasn't a connection that we initiated
  }

/*
*  Standard peer API producer once connection is established
*/
static int nbMsgPeerProducer(nbCELL context,nbPeer *peer,void *handle){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerProducer: called");
  if(!(msgcabal->mode&NB_MSG_CABAL_MODE_SERVER)){ // 
    nbLogMsg(context,0,'W',"nbMsgPeerProducer: Cabal not in server mode - not expecting request for messages - ignoring for now");
    return(0);
    }
  if(!msgnode->msgsub){
    nbLogMsg(context,0,'W',"nbMsgPeerProducer: Something funny. Called to get more data but no subscription");
    return(-1);
    }
  nbMsgCachePublish(context,msgnode->msgsub);
  return(0);
  }

/*
*  Standard peer API consumer once a connection is established
*/
static int nbMsgPeerConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;
  nbMsgRec *msgrec;
  int state;
  int rc;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerConsumer: called - len=%d",len);
  if(len<=0){
    nbLogMsg(context,0,'E',"nbMsgPeerConsumer: Connection %s shutting down - cabal %s node %s peer %s",peer->tls->uriMap[peer->tls->uriIndex].uri,msgcabal->cabalName,msgcabal->node->name,msgnode->name);
    nbLogMsg(context,0,'T',"nbMsgPeerProducer: Cabal %s node %s connection is shutting down",msgcabal->cabalName,msgnode->name);
    msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
    return(-1);
    }
  if(!(msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT)){ // server
    nbLogMsg(context,0,'E',"nbMsgPeerConsumer: Cabal not expecting messages - not in client mode - shutting down");
    if(msgcabal->mode&NB_MSG_CABAL_MODE_SERVER) nbMsgCacheCancel(context,msgnode->msgsub);
    return(-1);
    }
  msgrec=(nbMsgRec *)data;
  if(msgrec->type!=NB_MSG_REC_TYPE_MESSAGE){
    nbLogMsg(context,0,'E',"nbMsgPeerConsumer: Fatal error - invalid message record type %2.2x",msgrec->type);
    exit(1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerConsumer: calling nbMsgLogWriteReplica msgrec=%p",msgrec);
  state=nbMsgLogWriteReplica(context,msgnode->msgcabal->msglog,msgrec);
  if(state&NB_MSG_STATE_PROCESS){
    rc=(*msgnode->msgcabal->handler)(context,msgnode->msgcabal->handle,msgrec);
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerConsumer: rc=%d from client message handler",rc);
    }
  return(0);
  }

// The state record as we use it here will be replaced by a subscription
// message and we will not need a StateConsumer routine since it will be
// handled by the normal consumer function 
/*
*  Produce state record - start flow under old protocol
*/
static int nbMsgPeerStateProducer(nbCELL context,nbPeer *peer,void *handle){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;
  int msglen;

  //nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: called msgcabal=%p",msgcabal);
  //nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: calledd msgcabal->msglog=%p",msgcabal->msglog);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: calling nbMsgLogStateToRecord msgnode=%p",msgnode);
  msglen=nbMsgLogStateToRecord(context,msgcabal->msglog,msgcabal->cntlMsgBuf,NB_MSG_NODE_BUFLEN); // gen state record
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: sending state record  msglen=%d",msglen);
  if(nbPeerSend(context,peer,msgcabal->cntlMsgBuf,msglen)){ // send state record
    nbLogMsg(context,0,'E',"nbMsgPeerStateProducer: unable to send state record - shutting down connection");
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: State sent - handing off to nbMsgPeerProducer");
  nbPeerModify(context,peer,handle,nbMsgPeerProducer,nbMsgPeerConsumer,nbMsgPeerShutdown);
  msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
  return(0);
  }

// let's rename this and move up so we don't have to declare it
int nbMsgPeerCacheMsgHandler(nbCELL context,void *handle,nbMsgRec *msgrec);
/*
*  Read state record - anything else is bad (old protocol)
*/
static int nbMsgPeerStateConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;
  nbMsgState *msgstate;

  if(len<0){
    nbLogMsg(context,0,'E',"nbMsgPeerStateConsumer: Connection %s shutting down - cabal %s node %s peer %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
    return(0);
    }
  if(!(msgcabal->mode&NB_MSG_CABAL_MODE_SERVER)){ //  Not server mode
    nbLogMsg(context,0,'E',"nbMsgPeerStateConsumer: Not expecting state record - not in server mode  - shutting down connection");
    return(-1);
    }
  // handle the state record here - data is full message record
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: calling nbMsgLogStateFromRecord");
  msgstate=nbMsgLogStateFromRecord(context,(nbMsgRec *)data);
  if(!msgstate){
    nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: Unable to get state from state record");
    return(-1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: calling nbMsgCacheSubscribe");
  msgnode->msgsub=nbMsgCacheSubscribe(context,msgcabal->msgcache,msgstate,msgnode,nbMsgPeerCacheMsgHandler);
  if(!msgnode->msgsub){
    nbLogMsg(context,0,'E',"Unable to subscribe to message cache");
    return(-1);
    }
  nbPeerModify(context,peer,handle,nbMsgPeerProducer,nbMsgPeerConsumer,nbMsgPeerShutdown);
  msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
  return(0);
  }

static int nbMsgPeerHelloConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len);
/*
*  Produce hello message from peer requesting connection
*/
static int nbMsgPeerHelloProducer(nbCELL context,nbPeer *peer,void *handle){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;
  
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerHelloProducer: called for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  if(nbPeerSend(context,peer,(char *)&msgcabal->node->msgnoderec,sizeof(nbMsgNodeRec))!=0){
    nbLogMsg(context,0,'E',"nbMsgPeerHelloProducer: Unable to send node record");
    return(-1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerHelloProducer: Handing off to nbMsgPeerHelloConsumer");
  nbPeerModify(context,peer,msgnode,nbMsgPeerNullProducer,nbMsgPeerHelloConsumer,nbMsgPeerShutdown);
  return(0);
  }

/*
*  Consume hello message
*/
static int nbMsgPeerHelloConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgCabal *msgcabal=msgnode->msgcabal;

  if(len<0){
    nbLogMsg(context,0,'E',"nbMsgPeerHelloConsumer: Connection %s shutting down - cabal %s node %s peer %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
    return(0);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: called for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  // include code here to check the node record
  // if bad return -1
  //nbLogMsg(context,0,'T',"verify msgcabal=%p",msgcabal);
  //nbLogMsg(context,0,'T',"verify msgcabal->mode=%p",msgcabal->mode);
  //nbLogMsg(context,0,'T',"verify msgcabal->mode=%2.2x",msgcabal->mode);
  if(msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT){ // client
    nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: Handing client off to nbMsgPeerStateProducer");
    nbPeerModify(context,peer,msgnode,nbMsgPeerStateProducer,nbMsgPeerConsumer,nbMsgPeerShutdown);
    return(nbMsgPeerStateProducer(context,peer,msgnode));
    }
  else{
    nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: Handing server off to nbMsgPeerStateConsumer");
    nbPeerModify(context,peer,msgnode,nbMsgPeerNullProducer,nbMsgPeerStateConsumer,nbMsgPeerShutdown);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: returning for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  return(0);
  }

static int nbMsgCabalAcceptHelloConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len);
/*
*  Produce hello message from peer accepting connection
*/
static int nbMsgCabalAcceptHelloProducer(nbCELL context,nbPeer *peer,void *handle){
  nbMsgCabal *msgcabal=(nbMsgCabal *)handle;

  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for cabal=%p peer=%p ",msgcabal,peer);
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for peer->tls=%p",peer->tls);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for cabal %s uri %s",msgcabal->cabalName,nbTlsGetUri(peer->tls));
  if(nbPeerSend(context,peer,(char *)&msgcabal->node->msgnoderec,sizeof(nbMsgNodeRec))!=0){ // send nodeID record;
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloProducer: unable to write node record - %s",strerror(errno));
    return(-1);
    }
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: Server handing off to nbMsgCabalAcceptHelloConsumer");
  nbPeerModify(context,peer,msgcabal,nbMsgPeerNullProducer,nbMsgCabalAcceptHelloConsumer,nbMsgPeerAcceptShutdown);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: returning");
  return(0);
  }

/*
*  Consume hello message
*/
static int nbMsgCabalAcceptHelloConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len){
  nbMsgCabal *msgcabal=(nbMsgCabal *)handle;
  nbMsgNode *msgnode;
  char *nodeName;

  if(len<0){
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Connection %s shutting down - cabal %s node %s",peer->tls->uriMap[peer->tls->uriIndex].uri,msgcabal->cabalName,msgcabal->node->name);
    return(0);
    }
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: called for cabal %s",msgcabal->cabalName);
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: buffer received - len=%d",len);
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: size expecting - size=%d",sizeof(nbMsgNodeRec));
  if(len!=sizeof(nbMsgNodeRec)){
    nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: unexpected size of node record - %d - expecting %d",len,sizeof(nbMsgNodeRec));
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: data=%p",data);
  nodeName=(char *)data;
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: nodeName=%s",nodeName);

  // look up the node here and then switch to using the node as the session handle
  // check for reasonable data - this will change to a more structured record anyway
  for(msgnode=msgcabal->node->next;msgnode && msgnode!=msgcabal->node && (strcmp(msgnode->name,nodeName) || (msgcabal->mode&NB_MSG_CABAL_MODE_SERVER && msgnode->type&NB_MSG_NODE_TYPE_SERVER) || (msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT && msgnode->type&NB_MSG_NODE_TYPE_CLIENT));msgnode=msgnode->next);
  if(!msgnode || msgnode==msgcabal->node){
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Unable to locate connecting node %s",(char *)data);
    return(-1);
    }
  if(msgnode->state!=NB_MSG_NODE_STATE_DISCONNECTED){
    // 2012-01-04 eat - modified to accept replacement connections
    //   The original idea was to avoid a second process stealing a connection from connected process.
    //   However, there are cases where a client disconnects without the server getting notified.  The
    //   connection on the server side then prevented the client from reconnecting.  This logic has
    //   been modified to allow a new connection to replace an existing connection.
    //nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Node %s is not in a disconnected state - %d - shutting down new connection",msgnode->name,msgnode->state);
    //return(-1);
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Node %s is not in a disconnected state - %d - shutting down old connection",msgnode->name,msgnode->state);
    nbPeerShutdown(context,msgnode->peer,0);
    msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
    }
  nbLogMsg(context,0,'T',"Cabal node %s connecting",msgnode->name);
  // should check to make sure we aren't stomping on an old peer here
  msgnode->peer=peer;
  msgnode->state=NB_MSG_NODE_STATE_CONNECTING;
  if(msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT){ // client
    nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: Handing client off to state producer");
    nbPeerModify(context,peer,msgnode,nbMsgPeerStateProducer,NULL,nbMsgPeerShutdown);
    // force it - this is goofy
    return(nbMsgPeerStateProducer(context,peer,msgnode));
    }
  else{
    nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: Handing server off to state consumer");
    // nbPeerModify(context,peer,msgnode,NULL,nbMsgPeerStateConsumer,nbMsgPeerShutdown);
    nbPeerModify(context,peer,msgnode,nbMsgPeerNullProducer,nbMsgPeerStateConsumer,nbMsgPeerShutdown);
    }
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: Setting connecting state");
  return(0);
  }

/*
*  Create a message cabal node
*
*  myNodeType   - hub, spoke, source, or sink
*
*    TYPE     LOOKS FOR
*    ------   -------------
*    hub      hub and spoke
*    spoke    hub
*    source   sink
*    sink     source
*    0        anything
*
*  serviceType  - client, server, or peer
*/
nbMsgNode *nbMsgNodeCreate(nbCELL context,char *cabalName,char *nodeName,nbCELL nodeContext,int myNodeType,int serviceType){
  nbMsgNode *msgnode;
  int        nodeNumber;
  char      *uri;
  char      *nodeType;
  int        type=0;
  char      *serviceName;

  nbLogMsg(context,0,'T',"nbMsgNodeCreate: called with cabal \"%s\" node \"%s\"",cabalName,nodeName);
  if(strlen(nodeName)>=NB_MSG_NAMESIZE){
    nbLogMsg(context,0,'T',"Cabal \"%s\" node \"%s\" name too long - limit is %d",cabalName,nodeName,NB_MSG_NAMESIZE-1);
    return(NULL);
    }
  nodeNumber=nbTermOptionInteger(nodeContext,"id",-1);
  if(nodeNumber<0 || nodeNumber>255){
    nbLogMsg(context,0,'T',"Cabal \"%s\" node \"%s\" id %d is out of range 0<=n<=255",cabalName,nodeName,nodeNumber);
    return(NULL);
    }
  nodeType=nbTermOptionString(nodeContext,"type","spoke");
  if(!nodeType){
    nbLogMsg(context,0,'E',"Cabal \"%s\" node \"%s\" type not define in context",cabalName,nodeName);
    return(NULL);
    }
  if(strcmp(nodeType,"sink")==0) type=NB_MSG_NODE_TYPE_SINK;
  else if(strcmp(nodeType,"source")==0) type=NB_MSG_NODE_TYPE_SOURCE;
  else if(strcmp(nodeType,"hub")==0) type=NB_MSG_NODE_TYPE_HUB;
  else if(strcmp(nodeType,"spoke")==0) type=NB_MSG_NODE_TYPE_SPOKE;
  else{
    nbLogMsg(context,0,'E',"Cabal \"%s\" node \"%s\" type \"%s\" not recognized",cabalName,nodeName,nodeType);
    return(NULL);
    }
  // filter out the nodes we don't need
  if(myNodeType){
    if(myNodeType&NB_MSG_NODE_TYPE_HUB){
      if(!(type&NB_MSG_NODE_TYPE_HUB || type&NB_MSG_NODE_TYPE_SPOKE)) return(NULL);
      if(serviceType&NB_MSG_NODE_TYPE_SERVER && type&NB_MSG_NODE_TYPE_SPOKE) return(NULL);
      }
    else if(myNodeType&NB_MSG_NODE_TYPE_SPOKE){
      if(!(type&NB_MSG_NODE_TYPE_HUB)) return(NULL);
      }
    else if(myNodeType&NB_MSG_NODE_TYPE_SINK){
      if(!(type&NB_MSG_NODE_TYPE_SOURCE)) return(NULL);
      }
    else if(myNodeType&NB_MSG_NODE_TYPE_SOURCE){
      if(!(type&NB_MSG_NODE_TYPE_SINK)) return(NULL);
      }
    }
  if(msgTrace) nbLogMsg(context,0,'T',"calling nbTermOptionString to get service URI");
  serviceName="peer";
  uri=nbTermOptionString(nodeContext,serviceName,"");
  if(*uri) type|=NB_MSG_NODE_TYPE_SERVER|NB_MSG_NODE_TYPE_CLIENT;
  else if(serviceType&NB_MSG_NODE_TYPE_SERVER){
    serviceName="server";
    uri=nbTermOptionString(nodeContext,serviceName,"");
    type|=NB_MSG_NODE_TYPE_SERVER;
    }
  else if(serviceType&NB_MSG_NODE_TYPE_CLIENT){
    serviceName="client";
    uri=nbTermOptionString(nodeContext,serviceName,"");
    type|=NB_MSG_NODE_TYPE_CLIENT;
    }
  msgnode=(nbMsgNode *)nbAlloc(sizeof(nbMsgNode));
  memset(msgnode,0,sizeof(nbMsgNode)); 
  msgnode->prior=msgnode;
  msgnode->next=msgnode;
  strcpy(msgnode->name,nodeName); // size should be checked before call
  msgnode->number=nodeNumber;
  msgnode->state=0;
  msgnode->type=type;
  msgnode->order=0xff;
  msgnode->dn=NULL;
  // construct a peer structure for connecting later 
  msgnode->peer4Connect=nbPeerConstruct(context,1,serviceName,"",nodeContext,msgnode,nbMsgPeerHelloProducer,NULL,nbMsgPeerShutdown);
  msgnode->peer=msgnode->peer4Connect;  // will be replaced when we accept a connection from a peer
  if(!msgnode->peer){
    nbLogMsg(context,0,'E',"nbMsgNodeCreate: Unable to construct peer");
    }
  strcpy(msgnode->msgnoderec.name,msgnode->name);
  nbLogMsg(context,0,'T',"nbMsgNodeCreate: returning with uri=%s",uri);
  return(msgnode);
  }

void nbMsgCabalPrint(nbMsgCabal *msgcabal){
  nbMsgNode *msgnode;
  int count=0;
  for(msgnode=msgcabal->node->next;msgnode!=msgcabal->node && count<256;msgnode=msgnode->next){
    outPut("mode=%2.2x node=%s number=%d order=%d type=%2.2x state=%2.2x downTime=%d\n",msgcabal->mode,msgnode->name,msgnode->number,msgnode->order,msgnode->type,msgnode->state,msgnode->downTime);
    }
  }

/*
*  This is a scheduled routine to attempt connections to peers as required
*/
void nbMsgCabalRetry(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell){
  nbMsgCabal *msgcabal=(nbMsgCabal *)nodeHandle;
  
  nbMsgCabalEnable(context,msgcabal);
  }

/* 
*  Create a message cabal
*
*  Modeflags: see NB_MSG_CABAL_MODE_*
*/
nbMsgCabal *nbMsgCabalAlloc(nbCELL context,char *cabalName,char *nodeName,int mode){
  nbMsgCabal *msgcabal;
  nbCELL     cabalContext,nodeContext,tlsContext;
  char *ring;
  char *cursor,*delim;
  char peerName[NB_MSG_NAMESIZE];
  nbMsgNode *peerNode;
  nbMsgNode *anchor=NULL; // first in list at time self is encountered in ring
  int myNodeType;
  int serviceType;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalAlloc: called cabal=%s node=%s mode=%d",cabalName,nodeName,mode);
  cabalContext=nbTermLocate(context,cabalName);
  if(!cabalContext){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Cabal \"%s\" not defined in context",cabalName);
    return(NULL);
    }
  nodeContext=nbTermLocate(cabalContext,nodeName);
  if(!nodeContext){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Node \"%s\" not defined in context",nodeName);
    return(NULL);
    }
  ring=nbTermOptionString(nodeContext,"ring","");
  if(!ring || !*ring){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Node \"%s\" has no ring - prototype requirement",nodeName);
    return(NULL);
    }
  if(strlen(cabalName)>=NB_MSG_NAMESIZE){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Cabal name \"%s\" too long - limit is %d",cabalName,NB_MSG_NAMESIZE-1);
    return(NULL);
    }
  msgcabal=(nbMsgCabal *)nbAlloc(sizeof(nbMsgCabal));
  memset(msgcabal,0,sizeof(nbMsgCabal));
  msgcabal->mode=mode;
  msgcabal->cntlMsgBuf=(unsigned char *)nbAlloc(NB_MSG_CABAL_BUFLEN);
  strcpy(msgcabal->cabalName,cabalName);
  msgcabal->node=nbMsgNodeCreate(context,cabalName,nodeName,nodeContext,0,mode);
  if(!msgcabal->node){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Cabal \"%s\" has no node named \"%s\"",cabalName,nodeName);
    return(NULL);
    }
  myNodeType=msgcabal->node->type;
  msgcabal->node->msgcabal=msgcabal;
  msgcabal->nodeCount=0;  
  cursor=ring;
  while(*cursor){
    delim=strchr(cursor,',');
    if(!delim) delim=strchr(cursor,0);
    if(delim-cursor>=NB_MSG_NAMESIZE){
      nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Node name \"%s\" too long - limit is %d",nodeName,NB_MSG_NAMESIZE-1);
      // free stuff here
      return(NULL);
      }
    strncpy(peerName,cursor,delim-cursor); // length checked
    *(peerName+(delim-cursor))=0;
    cursor=delim;
    if(*cursor) cursor++; // step over comma
    if(strcmp(peerName,nodeName)==0){
      anchor=msgcabal->node->next;
      continue;  // don't reload self
      }
    nodeContext=nbTermLocate(cabalContext,peerName);
    if(!nodeContext){
      nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Node \"%s\" not defined in context",peerName);
      // free stuff here
      return(NULL);
      }
    for(serviceType=1;serviceType<3;serviceType++){
      // Servers look for clients, clients look for servers, and peers look for both and satisfy both
      if((mode&NB_MSG_CABAL_MODE_SERVER && serviceType==1) || (mode&NB_MSG_CABAL_MODE_CLIENT && serviceType==2)){
        peerNode=nbMsgNodeCreate(context,cabalName,peerName,nodeContext,myNodeType,serviceType); 
        if(peerNode){
          peerNode->msgcabal=msgcabal;
          peerNode->order=msgcabal->nodeCount;
          if(anchor){
            peerNode->next=anchor;
            peerNode->prior=anchor->prior;
            anchor->prior->next=peerNode;
            anchor->prior=peerNode;
            }
          else{
            peerNode->next=msgcabal->node;
            peerNode->prior=msgcabal->node->prior;
            msgcabal->node->prior->next=peerNode;
            msgcabal->node->prior=peerNode;
            }
          // if we look for a client and find a peer, we don't need to look for a server
          if(peerNode->type&NB_MSG_NODE_TYPE_SERVER) continue;
          }
        }
      }
    msgcabal->nodeCount++;
    }
  if(msgTrace) nbMsgCabalPrint(msgcabal);
  // some types of nodes need to listen for a connection - source (server) and hub (client and server)
  if(msgcabal->node->type&(NB_MSG_NODE_TYPE_HUB|NB_MSG_NODE_TYPE_SOURCE|NB_MSG_NODE_TYPE_SINK)){ 
    // It is my beleive that if we modify nbMsgNodeCreate to support the
    // a uriName parameter, we can use the root node's peer and not need
    // to create one here.
  
    // Set up and register listener for TLS connections
    tlsContext=nbTermLocate(context,"server");
    if(!tlsContext){
      nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Term \"server\" not defined");
      nbMsgCabalFree(context,msgcabal);
      return(NULL);
      }
  
    // construct a listening peer structure - should the consumer be null here?
    msgcabal->peer=nbPeerConstruct(context,0,"uri","",tlsContext,msgcabal,nbMsgCabalAcceptHelloProducer,nbMsgCabalAcceptHelloConsumer,nbMsgPeerAcceptShutdown);
    if(!msgcabal->peer){
      nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Peer structure to listen was not created");
      nbMsgCabalFree(context,msgcabal);
      return(NULL);
      }
    if(nbPeerListen(context,msgcabal->peer)!=0){ // start listening
      nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Unable to listen to peer");
      nbMsgCabalFree(context,msgcabal);
      return(NULL);
      }
    nbLogMsg(context,0,'I',"Listening on %s",msgcabal->peer->tls->uriMap[0].uri);
    }
  msgcabal->synapse=nbSynapseOpen(context,NULL,msgcabal,NULL,nbMsgCabalRetry);
  return(msgcabal);
  }

int nbMsgCabalDisable(nbCELL context,nbMsgCabal *msgcabal){
  nbSynapseSetTimer(context,msgcabal->synapse,0);
  return(0);
  }

int nbMsgCabalFree(nbCELL context,nbMsgCabal *msgcabal){
  return(0);
  }

/*
*  Open a message server
*/
nbMsgCabal *nbMsgCabalServer(nbCELL context,char *cabalName,char *nodeName){
  nbMsgCabal *msgcabal;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalServer: called for cabal=%s node=%s",cabalName,nodeName);
  msgcabal=nbMsgCabalAlloc(context,cabalName,nodeName,NB_MSG_CABAL_MODE_SERVER); // server mode
  if(!msgcabal) return(NULL);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalServer: msgcabal->node->number=%d",msgcabal->node->number);
  if(msgcabal->node->number<0){
    nbLogMsg(context,0,'E',"nbMsgCabalServer: Node \"%s\" required number not defined in context",nodeName);
    return(NULL);
    }
  msgcabal->msgcache=nbMsgCacheAlloc(context,cabalName,nodeName,msgcabal->node->number,2*1024*1024);
  if(!msgcabal->msgcache){
    nbLogMsg(context,0,'T',"nbMsgCabalServer: Unable to alloc cache for cabal '%s' node '%s' - terminating",cabalName,nodeName);
    nbLogFlush(context);
    exit(1);
    }
  return(msgcabal);
  }

/*
*  Open a message client
*
*    When called without a message state, the state is defined by the message log
*    and we don't pass any messages from the log to the message handler.
*/
nbMsgCabal *nbMsgCabalClient(nbCELL context,char *cabalName,char *nodeName,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
  nbMsgCabal *msgcabal;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalClient: called for cabal=%s node=%s",cabalName,nodeName);
  msgcabal=nbMsgCabalAlloc(context,cabalName,nodeName,NB_MSG_CABAL_MODE_CLIENT); // client mode
  if(!msgcabal) return(NULL);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCabalCient: msgcabal->node->number=%d",msgcabal->node->number);
  msgcabal->handle=handle;
  msgcabal->handler=handler;
  return(msgcabal);
  }

/*
*  Synchronize client with own message log
*
*        nbMsgCabalFree(context,msgcabal);
*/
int nbMsgCabalClientSync(nbCELL context,nbMsgCabal *msgcabal,nbMsgState *msgstate){
  nbMsgLog *msglog;
  int state;
  int rc;

  if(!msgcabal){
    nbLogMsg(context,0,'E',"nbMsgCabalClientSync: called with null msgcabal");
    return(-1);
    }
  msglog=nbMsgLogOpen(context,msgcabal->cabalName,msgcabal->node->name,msgcabal->node->number,"",NB_MSG_MODE_PRODUCER,msgstate);
  if(!msglog){
    nbLogMsg(context,0,'E',"nbMsgCabalClientSync: msgcabal has null msglog");
    return(-1);
    }
  msgcabal->msglog=msglog;
  while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
    //nbLogMsg(context,0,'T',"nbMsgCabalClientSync: return from nbMsgLogRead state=%d",state);
    if(msgstate && state&NB_MSG_STATE_PROCESS){  // handle new message records
      nbLogMsg(context,0,'T',"nbMsgCabalClientSync: calling message handler");
      // consider using the root node's handle and handler
      if((rc=(*msgcabal->handler)(context,msgcabal->handle,msglog->msgrec))!=0){
        nbLogMsg(context,0,'I',"Message handler return code=%d",rc);
        return(-1);
        }
      }
    }
  if(nbMsgLogProduce(context,msglog,10*1024*1024)!=0){
    nbLogMsg(context,0,'E',"nbMsgCabalClient: Unable to switch to producer mode");
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgCabalClient: End of log - cabal=%s node=%s",msgcabal->cabalName,msgcabal->node->name);
  return(0);
  }

/*
*  Open a message cabal
*
*  Mode:
*    NB_MSG_CABAL_MODE_CLIENT   - Connect to and accept connections from appropriate server(s)
*    NB_MSG_CABAL_MODE_SERVER   - Connect to and accept connections from appropriate client(s)
*    NB_MSG_CABAL_MODE_PEER     - Client and server (not yet implemented)
*
*  The connection goal of a node depends on the type defined in the cabal configuration
*
*    NB_MSG_NODE_TYPE_HUB       - client connects to hub left, server connects to hub right
*    NB_MSG_NODE_TYPE_SPOKE     - client connects to hub left, has no server
*    NB_MSG_NODE_TYPE_SOURCE    - has no client, server makes no connections
*    NB_MSG_NODE_TYPE_SINK      - client connects to all sources, has no server
*
*  The cabal configuration node is identified by the cabalName relative to the specified context.
*  The individual node configuration node is identified by the nodeName.
*
*  A spoke or sink node may have a node number of 0 because they don't contribute messages
*  A source or hub node must have a node number from 1 to 255
*
*/
nbMsgCabal *nbMsgCabalOpen(nbCELL context,int mode,char *cabalName,char *nodeName,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
  nbMsgCabal *msgcabal;
  //nbMsgState *msgstate;
  nbMsgLog *msglog;
  int state;
  int rc;

  msgcabal=nbMsgCabalAlloc(context,cabalName,nodeName,mode); // 
  if(!msgcabal){
    nbLogMsg(context,0,'E',"Unable to allocate cabal structure");
    return(NULL);
    }
  msgcabal->handle=handle;
  msgcabal->handler=handler;
  //msgstate=nbMsgStateCreate(context);
  nbLogMsg(context,0,'T',"msgcabal->node->number=%d",msgcabal->node->number);
  if(mode&NB_MSG_CABAL_MODE_CLIENT){
    msglog=nbMsgLogOpen(context,cabalName,nodeName,msgcabal->node->number,"",NB_MSG_MODE_PRODUCER,msgstate);
    if(!msglog){
      nbLogMsg(context,0,'E',"Unable to open message log");
      return(NULL);
      }
    msgcabal->msglog=msglog;
    while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
      //nbLogMsg(context,0,'T',"nbMsgCabalClient: return from nbMsgLogRead state=%d",state);
      if(msgstate && state&NB_MSG_STATE_PROCESS){  // handle new message records
        nbLogMsg(context,0,'T',"nbMsgCabalClient: calling message handler");
        // consider using the root node's handle and handler
        if((rc=(*msgcabal->handler)(context,msgcabal->handle,msglog->msgrec))!=0){
          nbLogMsg(context,0,'I',"Message handler return code=%d",rc);
          nbMsgCabalFree(context,msgcabal);
          return(NULL);
          }
        }
      }
    if(nbMsgLogProduce(context,msglog,10*1024*1024)!=0){
      nbLogMsg(context,0,'E',"nbMsgCabalClient: Unable to switch to producer mode");
      nbMsgCabalFree(context,msgcabal);
      return(NULL);
      }
    nbLogMsg(context,0,'T',"nbMsgCabalClient: End of log - cabal=%s node=%s",cabalName,nodeName);
    }
  if(mode&NB_MSG_CABAL_MODE_SERVER){
    msgcabal->msgcache=nbMsgCacheAlloc(context,cabalName,nodeName,msgcabal->node->number,2*1024*1024);
    }
  return(msgcabal);
  }

/*
*  Handle message from cache
*
*    We check to see if the message has already been at the client
*    node by looking in the message path.  This is required to 
*    avoid sending messages back to the originator and intermediate
*    nodes.  The message state test performed by the message cache
*    is not sufficient to cover messages the occurred at the peer
*    from other sources since we first learned of the peer's state.
*
*  Returns: 0 - handled, 1 - not ready (will call back when ready)
*
*/
int nbMsgPeerCacheMsgHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbMsgNode *msgnode=(nbMsgNode *)handle;
  nbMsgId *msgid;
  int msgids;
  int size;

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerCacheMsgHandler: called");
  size=(msgrec->len[0]<<8)|msgrec->len[1];
  if(!size){
    nbLogMsg(context,0,'L',"nbMsgPeerCacheMsgHandler: we should not receive a zero length record - terminating");
    exit(1);
    }
  // ignore messages that have been at the peer node
  if(msgnode->number){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerCacheMsgHandler: checking msgid for visit at node %d",msgnode->number);
    msgid=&msgrec->si;
    if(msgid->node==msgnode->number) return(0);
    msgid+=2;
    for(msgids=msgrec->msgids;msgids;msgids--){
      if(msgid->node==msgnode->number) return(0);
      }
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerCacheMsgHandler: msgid not seen at %d",msgnode->number);
    }
  else if(msgTrace) nbLogMsg(context,0,'T',"nbMsgPeerCacheMsgHandler: not checking msgid because target node is zero");
  return(nbPeerSend(context,msgnode->peer,(unsigned char *)msgrec,size));
  }

/*
*  Enable peer connections
*
*    This function is called to get the cabal connections in a preferred
*    state.  Once in a preferred state, we don't need to call this function
*    unless a connection is lost.
*
*    We are using a simplified strategy for dropping extra connections. If we
*    establish a "more preferred" connection, we shutdown any "less preferred"
*    connection that we established.  We leave it to peers to decide when to
*    shutdown connections they establish.
*
*/
int nbMsgCabalEnable(nbCELL context,nbMsgCabal *msgcabal){
  nbMsgNode *msgnode;
  int count=0;
  int limit=255;      // avoid infinite loop in node list
  int connected;      // -1 - found no connected or connecting peer, 0 - found connecting peer, 1 - found connected peer 
  int expirationTime;
  int preferred=1;    // assume we are in a prefered state
  int satisfied=1;    // assume we are in a satisfied state (used for ring topology only)
  time_t utime;

  if(msgTrace){
     nbLogMsg(context,0,'T',"nbMsgCabalEnable: called for cabal %s node %s mode=%2.2x type=%2.2x",msgcabal->cabalName,msgcabal->node->name,msgcabal->mode,msgcabal->node->type);
    nbMsgCabalPrint(msgcabal);
    }

  nbSynapseSetTimer(context,msgcabal->synapse,0); // cancel previous timer
  time(&utime);
  expirationTime=utime;
  expirationTime-=30;  // wait 30 seconds before connecting after a disconnect

  // Handle fan topology sink (client) and source (server)
  if(msgcabal->node->type&NB_MSG_NODE_TYPE_FAN){
    if(msgcabal->node->type&NB_MSG_NODE_TYPE_SOURCE){
      //nbLogMsg(context,0,'T',"nbMsgCabalEnable: source node waits for connection from sink nodes");
      //return(0);
      // 2011-02-06 eat - experimenting with source nodes connecting to sink nodes
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: source server for cabal %s node %s",msgcabal->cabalName,msgcabal->node->name);
      for(count=0,msgnode=msgcabal->node->next;msgnode!=msgcabal->node && count<limit;msgnode=msgnode->next){
        // We can unify this and the next outer block as follows
        // if((msgcabal->node->type&NB_MSG_NODE_TYPE_SOURCE && 
        //       msgnode->type&NB_MSG_NODE_TYPE_SINK && msgnode->type&NB_MSG_NODE_TYPE_CLIENT) ||
        //    (msgcabal->node->type&NB_MSG_NODE_TYPE_SINK &&
        //       msgnode->type&NB_MSG_NODE_TYPE_SOURCE && msgnode->type&NB_MSG_NODE_TYPE_SERVER))
        if(msgnode->peer->tls && msgnode->type&NB_MSG_NODE_TYPE_SINK && msgnode->type&NB_MSG_NODE_TYPE_CLIENT){
          if(msgnode->state!=NB_MSG_NODE_STATE_CONNECTED) preferred=0;
          if(msgnode->state==NB_MSG_NODE_STATE_DISCONNECTED && msgnode->downTime<expirationTime){
            // we only want to connect from a disconnected state
            nbLogMsg(context,0,'T',"nbMsgCabalEnable: calling nbPeerConnect for cabal %s source node %s to sink node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
            if(msgnode->peer!=msgnode->peer4Connect){
              nbPeerDestroy(context,msgnode->peer);
              msgnode->peer=msgnode->peer4Connect;
              }
            msgnode->state=NB_MSG_NODE_STATE_CONNECTING;
            connected=nbPeerConnect(context,msgnode->peer,msgnode,nbMsgPeerHelloProducer,NULL,nbMsgPeerShutdown);
            if(connected<0){
              nbLogMsg(context,0,'T',"nbMsgCabalEnable: nbPeerConnect failed for cabal %s source node %s to sink node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
              msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
              msgnode->downTime=utime;
              preferred=0;
              }
            else if(connected==1) msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
            else preferred=0;
            count++;
            }
          }
        }
      }
    // A sink node tries to connect to all source nodes (we don't connect to hubs---thats what spoke nodes do)
    if(msgcabal->node->type&NB_MSG_NODE_TYPE_SINK){
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: sink client for cabal %s node %s",msgcabal->cabalName,msgcabal->node->name);
      for(count=0,msgnode=msgcabal->node->next;msgnode!=msgcabal->node && count<limit;msgnode=msgnode->next){
        if(msgnode->peer->tls && msgnode->type&NB_MSG_NODE_TYPE_SOURCE && msgnode->type&NB_MSG_NODE_TYPE_SERVER){
          if(msgnode->state!=NB_MSG_NODE_STATE_CONNECTED) preferred=0;
          if(msgnode->state==NB_MSG_NODE_STATE_DISCONNECTED && msgnode->downTime<expirationTime){
            // we only want to connect from a disconnected state 
            nbLogMsg(context,0,'T',"nbMsgCabalEnable: calling nbPeerConnect for cabal %s sink node %s to source node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
            if(msgnode->peer!=msgnode->peer4Connect){
              nbPeerDestroy(context,msgnode->peer);
              msgnode->peer=msgnode->peer4Connect;
              }
            msgnode->state=NB_MSG_NODE_STATE_CONNECTING;
            connected=nbPeerConnect(context,msgnode->peer,msgnode,nbMsgPeerHelloProducer,NULL,nbMsgPeerShutdown);
            if(connected<0){
              nbLogMsg(context,0,'T',"nbMsgCabalEnable: nbPeerConnect failed for cabal %s sink node %s to source node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
              msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
              msgnode->downTime=utime;
              preferred=0;
              }
            else if(connected==1) msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
            else preferred=0;
            count++;
            }
          }
        }
      }
    }
  // Handle ring topology hub (client and/or server) and spoke (client)
  if(msgcabal->node->type&NB_MSG_NODE_TYPE_RING){
    // In server or peer mode we try to connect to one hub node in front of us
    if(msgcabal->mode&NB_MSG_CABAL_MODE_SERVER){  // server tries to connect to clients
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: hub server for cabal %s node %s",msgcabal->cabalName,msgcabal->node->name);
      for(count=0,connected=-1,msgnode=msgcabal->node->next;connected==-1 && msgnode!=msgcabal->node && count<limit;msgnode=msgnode->next){
        if(msgnode->peer->tls && msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_CLIENT){
          if(msgnode->state==NB_MSG_NODE_STATE_CONNECTED) connected=1;
          else if(msgnode->state==NB_MSG_NODE_STATE_CONNECTING) connected=0;
          else if(msgnode->state==NB_MSG_NODE_STATE_DISCONNECTED && msgnode->downTime<expirationTime){
            // we only want to connect from a disconnected state that has aged sufficiently
            nbLogMsg(context,0,'T',"nbMsgCabalEnable: calling nbPeerConnect for cabal %s server node %s to client node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
            if(msgnode->peer!=msgnode->peer4Connect){
              nbPeerDestroy(context,msgnode->peer);
              msgnode->peer=msgnode->peer4Connect;
              }
            msgnode->state=NB_MSG_NODE_STATE_CONNECTING;
            connected=nbPeerConnect(context,msgnode->peer,msgnode,nbMsgPeerHelloProducer,NULL,nbMsgPeerShutdown);
            if(connected<0){
              nbLogMsg(context,0,'T',"nbMsgCabalEnable: nbPeerConnect failed for cabal %s client node %s to server node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
              msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
              msgnode->downTime=utime;
              preferred=0;
              }
            else if(connected==1) msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
            else preferred=0;
            count++;
            }
          else preferred=0;
          }
        }
      if(connected<1) satisfied=0;
      else{  // continue through list of nodes and disconnect any less prefered connection we initiated
        for(;msgnode!=msgcabal->node;msgnode=msgnode->next){
          if(msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_CLIENT){
            if(msgnode->state==NB_MSG_NODE_STATE_CONNECTED && msgnode->peer==msgnode->peer4Connect){ // we initiated the less preferred connection
              nbPeerShutdown(context,msgnode->peer,0);  // drop the extra connection - let peer re-establish if needed
              }
            }
          }
        }
      }
    // In client or peer mode we try to connect to one hub node behind us
    if(msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT){ // client tries to connect to server in reverse order
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: client for cabal %s node %s",msgcabal->cabalName,msgcabal->node->name);
      for(count=0,connected=-1,msgnode=msgcabal->node->prior;connected==-1 && msgnode!=msgcabal->node && count<limit;msgnode=msgnode->prior){
        if(msgnode->peer->tls && msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_SERVER){
          if(msgnode->state==NB_MSG_NODE_STATE_CONNECTED) connected=1;
          else if(msgnode->state==NB_MSG_NODE_STATE_CONNECTING) connected=0;
          else if(msgnode->state==NB_MSG_NODE_STATE_DISCONNECTED && msgnode->downTime<expirationTime){
            // we only want to connect from a disconnected state 
            nbLogMsg(context,0,'T',"nbMsgCabalEnable: calling nbPeerConnect for cabal %s client node %s to server node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
            if(msgnode->peer!=msgnode->peer4Connect){
              nbPeerDestroy(context,msgnode->peer);
              msgnode->peer=msgnode->peer4Connect;
              }
            msgnode->state=NB_MSG_NODE_STATE_CONNECTING;
            connected=nbPeerConnect(context,msgnode->peer,msgnode,nbMsgPeerHelloProducer,NULL,nbMsgPeerShutdown);
            if(connected<0){
              nbLogMsg(context,0,'T',"nbMsgCabalEnable: nbPeerConnect failed for cabal %s client node %s to server node %s",msgcabal->cabalName,msgcabal->node->name,msgnode->name);
              msgnode->state=NB_MSG_NODE_STATE_DISCONNECTED;
              msgnode->downTime=utime;
              preferred=0;
              }
            else if(connected==1) msgnode->state=NB_MSG_NODE_STATE_CONNECTED;
            else preferred=0;
            count++;
            }
          else preferred=0;
          }
        }
      if(connected<1) satisfied=0;
      else{  // continue through list of nodes and disconnect any less prefered connection we initiated
        for(;msgnode!=msgcabal->node;msgnode=msgnode->prior){
          if(msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_SERVER){
            if(msgnode->state==NB_MSG_NODE_STATE_CONNECTED && msgnode->peer==msgnode->peer4Connect){ // we initiated the less preferred connection
              nbPeerShutdown(context,msgnode->peer,0);  // drop the extra connection - let peer re-establish if needed
              }
            }
          }
        }
      }
    }
  if(count>=256){
    nbLogMsg(context,0,'L',"nbMsgCabalEnable: Corrupted node list - terminating");
    exit(1);
    }
  // If not in a preferred state let's set a medulla timer using synapse to check again later
  if(!preferred) nbSynapseSetTimer(context,msgcabal->synapse,30);
  return(0);
  }
