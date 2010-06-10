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
*   Log files within a log directory are named as follows, where T is the
*   UTC time when the log started.  The active log does not include the time
*   or count, but is renamed when a new active log is started. 
*
*     m.T.nbm 
*     m.nbm
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
*==============================================================================
*/
#include <ctype.h>
#include <nbi.h>
#if !defined(WIN32)
#include <sys/un.h>
#endif

int nbMsgUdpLocalClientSocket(nbMsgLog *msglog){
  if((msglog->socket=socket(AF_UNIX,SOCK_DGRAM,0))<0){
    fprintf(stderr,"nbMsgUdpClientSocket: Unable to get socket - %s\n",strerror(errno));
    msglog->socket=0;
    return(-1);
    }
  msglog->un_addr.sun_family=AF_UNIX;
  sprintf(msglog->un_addr.sun_path,"message/%s/%s/s.nbm",msglog->cabal,msglog->nodeName);
  return(0);
  }

int nbMsgUdpClientSend(nbCELL context,nbMsgLog *msglog,void *data,int len){
  nbLogMsg(context,0,'T',"nbMsgUdpClientSend: sending datagram message of length %d",len);
  if(sendto(msglog->socket,data,len,MSG_DONTWAIT,(struct sockaddr *)&msglog->un_addr,sizeof(struct sockaddr_un))<0){
    nbLogMsg(context,0,'E',"nbMsgUdpClientSend: sending datagram message - %s",strerror(errno));
    return(-1);
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
  for(node=0;node<256;node++){
    msgnum=&msgstate->msgnum[node];
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
  nbLogMsg(context,0,'T',"nbMsgStateCreate call malloc for %d bytes",sizeof(nbMsgState));
  state=(nbMsgState *)malloc(sizeof(nbMsgState));
  if(!state){
    nbLogMsg(context,0,'E',"Fatal error - %s - terminating",strerror(errno));
    exit(1);
    }
  memset(state,0,sizeof(nbMsgState));
  return(state);
  }

void nbMsgStateFree(nbCELL context,nbMsgState *msgState){
  free(msgState);
  }

/*
*  Set a state for an individual node
*/
int nbMsgStateSet(nbMsgState *state,int node,uint32_t time,uint32_t count){
  if(node<0 || node>NB_MSG_NODE_MAX){
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
  uint32_t time=ntohl(*(uint32_t *)&msgid->time);
  uint32_t count=ntohl(*(uint32_t *)&msgid->count);
  uint32_t countAhead=state->msgnum[node].count+1;
  if(!countAhead) countAhead++;  // skip over zero - is special count value
  nbLogMsg(context,0,'T',"nbMsgStateSetFromMsgId: node=%d countAhead=%u count=%u",node,countAhead,count);
  if(countAhead!=count) return(-1);
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
  uint32_t time=ntohl(*(uint32_t *)&msgid->time);
  uint32_t count=ntohl(*(uint32_t *)&msgid->count);
  if(time<state->msgnum[node].time) return(-1);
  if(time>state->msgnum[node].time) return(1);
  return(nbMsgCountCompare(count,state->msgnum[node].count));
  }


//==============================================================================
// Operations on message records
//==============================================================================

int nbMsgPrintHex(FILE *file,unsigned short len,unsigned char *buffer){
  unsigned char *cursor=buffer;

  for(cursor=buffer;cursor<buffer+len;cursor++){
    fprintf(file,"%2.2x",*cursor);
    }
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
    mTime=ntohl(*(uint32_t *)msgid->time);
    mCount=ntohl(*(uint32_t *)msgid->count);
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
      mTime=ntohl(*(uint32_t *)msgid->time);
      mCount=ntohl(*(uint32_t *)msgid->count);
      fprintf(file,"%u-%u-%u",mNode,mTime,mCount);
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
*    This function scans the msgid values in a message record
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
    recordTime=ntohl(*(uint32_t *)msgid->time);
    recordCount=ntohl(*(uint32_t *)msgid->count);
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
  tranTime=ntohl(*(uint32_t *)msgid->time);
  tranCount=ntohl(*(uint32_t *)msgid->count);
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
*    start of a message log file is sufficiently far enough back
*    to include the state of the program/process, pgmState. 
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
  nbMsgState *logState=msglog->logState;
  nbMsgState *pgmState=msglog->pgmState;

  node=msgid->node;
  mTime=ntohl(*(uint32_t *)msgid->time);
  mCount=ntohl(*(uint32_t *)msgid->count);
  if(mTime>pgmState->msgnum[node].time) return(0);
  else if(mTime==logState->msgnum[node].time && mCount>logState->msgnum[node].count) return(0);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    node=msgid->node;
    mTime=ntohl(*(uint32_t *)msgid->time);
    mCount=ntohl(*(uint32_t *)msgid->count);
    if(mTime>pgmState->msgnum[node].time) return(0);
    else if(mTime==pgmState->msgnum[node].time && mCount>pgmState->msgnum[node].count) return(0);
    msgid++;
    }
  return(1);  
  }

/*
*  Store a message id in network byte order
*/
void nbMsgIdStuff(nbMsgId *msgid,int node,uint32_t mTime,uint32_t mCount){
  msgid->node=node;
  msgid->time[3]=mTime&0xff; mTime=mTime>>8;
  msgid->time[2]=mTime&0xff; mTime=mTime>>8;
  msgid->time[1]=mTime&0xff; mTime=mTime>>8;
  msgid->time[0]=mTime&0xff;
  msgid->count[3]=mCount&0xff; mCount=mCount>>8;
  msgid->count[2]=mCount&0xff; mCount=mCount>>8;
  msgid->count[1]=mCount&0xff; mCount=mCount>>8;
  msgid->count[0]=mCount&0xff;
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
  nbLogMsg(context,0,'T',"nbMsgLogStateToRecord: Created state record");
  nbMsgStatePrint(stderr,msglog->logState,"Log state:");
  nbMsgStatePrint(stderr,msglog->pgmState,"Pgm state:");
  nbMsgPrint(stderr,msgrec);
  nbLogMsg(context,0,'T',"nbMsgLogStateToRecord: returning");
  return(len);
  }

nbMsgState *nbMsgLogStateFromRecord(nbCELL context,nbMsgRec *msgrec){
  nbMsgState *msgstate;
  nbMsgId *msgid;
  int msgids;
  int nodeIndex;

  msgstate=nbMsgStateCreate(context);
  msgid=&msgrec->si;
  nodeIndex=msgid->node;
  msgstate->msgnum[nodeIndex].time=ntohl(*(uint32_t *)&msgid->time);
  msgstate->msgnum[nodeIndex].count=ntohl(*(uint32_t *)&msgid->count);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    nodeIndex=msgid->node;
    msgstate->msgnum[nodeIndex].time=ntohl(*(uint32_t *)&msgid->time);
    msgstate->msgnum[nodeIndex].count=ntohl(*(uint32_t *)&msgid->count);
    msgid++;
    }
  nbMsgStatePrint(stderr,msgstate,"Client state:");
  nbMsgPrint(stderr,msgrec);
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
char *nbMsgHeaderExtract(nbMsgRec *msgrec,int node,uint32_t *tranTimeP,uint32_t *tranCountP,uint32_t *recordTimeP,uint32_t *recordCountP,uint32_t *fileTimeP,uint32_t *fileCountP){
  nbMsgId *msgid=&msgrec->si;

  nbMsgPrint(stderr,msgrec);
  if(msgrec->type!=NB_MSG_REC_TYPE_HEADER) return("msg type not header");
  if(msgrec->datatype!=NB_MSG_REC_DATA_ID) return("msg data type not ID");
  if(msgid->node!=node) return("state message id node does not match expected node");
  *tranTimeP=ntohl(*(uint32_t *)msgid->time);
  *tranCountP=ntohl(*(uint32_t *)msgid->count);
  msgid++;
  if(msgid->node!=node) return("log message id node does not match expected node");
  *recordTimeP=ntohl(*(uint32_t *)msgid->time);
  *recordCountP=ntohl(*(uint32_t *)msgid->count);
  msgid+=1+msgrec->msgids;
  if(msgid->node!=node) return("log message id node does not match expected node");
  *fileTimeP=ntohl(*(uint32_t *)msgid->time);
  *fileCountP=ntohl(*(uint32_t *)msgid->count);
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

  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: called with state=%x",msglog->state);
  if(msglog->state&NB_MSG_STATE_LOGEND){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Check for growth in cabal \"%s\" node %u file %s",msglog->cabal,msglog->node,msglog->filename);
    if(msglog->file){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Logic error - cabal \"%s\" node %u file %s - still open while log is in end-of-log state",msglog->cabal,msglog->node,msglog->filename);
      return(-1);
      }
    sprintf(filename,"message/%s/%s/m%10.10u.nbm",msglog->cabal,msglog->nodeName,msglog->fileCount);
    if((msglog->file=open(filename,O_RDONLY))<0){
      nbLogMsg(context,0,'E',"nbMsgLogRead: Unable to open file %s - %s",filename,strerror(errno));
      return(-1);
      }
    //if((pos=lseek(msglog->file,msglog->fileOffset,SEEK_SET))<0){
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
    sprintf(msglog->filename,"m%10.10d.nbm",msglog->fileCount);  
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
    fprintf(stderr,"nbMsgLogRead: 1 msglog->fileOffset=%d\n",msglog->fileOffset);
    cursor+=(*cursor<<8)|*(cursor+1);  // step to next record - over header
    msglog->msgrec=(nbMsgRec *)cursor;
    }
  else{
    cursor=(unsigned char *)msglog->msgrec;
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgLogRead: Step to next record at %p *cursor=%2.2x%2.2x",cursor,*cursor,*(cursor+1));
    msglog->fileOffset+=(*cursor<<8)|*(cursor+1);  // update file offset
    fprintf(stderr,"nbMsgLogRead: 2 msglog->fileOffset=%d\n",msglog->fileOffset);
    cursor+=(*cursor<<8)|*(cursor+1);  // step to next record
    msglog->msgrec=(nbMsgRec *)cursor;
    }
  bufend=msglog->msgbuf+msglog->msgbuflen;
  cursor=(unsigned char *)msglog->msgrec;
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
*    see if the caller has provided the time for a specific message log file mT.nbm.
*    If this file does not exist, we open m.nbm and chain back as far as necessary
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
nbMsgLog *nbMsgLogOpen(nbCELL context,char *cabal,char *nodeName,int node,char *basename,int mode,nbMsgState *pgmState){
  nbMsgLog *msglog;
  char filename[128];
  char cursorFilename[128];
  unsigned short int msglen;
  int msgbuflen;
  char *errStr;
  uint32_t tranTime,tranCount,recordTime,recordCount,fileTime,fileCount;
  char linkname[128];
  char linkedname[32];
  int linklen;
  nbMsgCursor msgcursor;

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
  if(node>NB_MSG_NODE_MAX){
    fprintf(stderr,"nbMsgLogOpen: Node number %d exceeds limit of %d\n",node,NB_MSG_NODE_MAX);
    return(NULL);
    }
  if(strlen(cabal)>sizeof(msglog->cabal)-1){
    fprintf(stderr,"nbMsgLogOpen: Message cabal name length exceeds %d bytes\n",(int)sizeof(msglog->cabal)-1);
    return(NULL);
    }
  if(strlen(nodeName)>sizeof(msglog->nodeName)-1){
    fprintf(stderr,"nbMsgLogOpen: Message node name length exceeds %d bytes\n",(int)sizeof(msglog->nodeName)-1);
    return(NULL);
    }
  // Get the file name to start with
  if(mode==NB_MSG_MODE_SINGLE){
    fprintf(stderr,"nbMsgLogOpen: called with option 1\n");
    if(strlen(basename)>sizeof(linkedname)-1){
      fprintf(stderr,"nbMsgLogOpen: file name length %d exceeds limit of %d\n",(int)strlen(basename),(int)sizeof(linkedname)-1);
      return(NULL);
      }
    strcpy(linkedname,basename);
    fprintf(stderr,"nbMsgLogOpen: linkedname=%s\n",linkedname);
    }
  else{
    sprintf(linkname,"message/%s/%s/m.nbm",cabal,nodeName);
    // change this to retry in a loop for a few times if not mode PRODUCER because nbMsgLogFileCreate may be removing and creating link
    linklen=readlink(linkname,linkedname,sizeof(linkedname));
    if(linklen<0){
      fprintf(stderr,"nbMsgLogOpen: Unable to read link %s - %s\n",linkname,strerror(errno));
      return(NULL);
      }
    if(linklen==sizeof(linkname)){
      fprintf(stderr,"nbMsgLogOpen: Symbolic link %s point to file name too long for buffer\n",linkname);
      return(NULL);
      }
    *(linkedname+linklen)=0;
    fprintf(stderr,"nbMsgLogOpen: Link is %s -> %s\n",linkname,linkedname);
    }
  // create a msglog structure
  msglog=(nbMsgLog *)nbAlloc(sizeof(nbMsgLog));
  memset(msglog,0,sizeof(nbMsgLog));
  strcpy(msglog->cabal,cabal);
  strcpy(msglog->nodeName,nodeName);
  msglog->node=node;
  strcpy(msglog->filename,linkedname);
  msglog->mode=mode;
  msglog->state=NB_MSG_STATE_INITIAL;
  //msglog->file=0;
  //msglog->maxfilesize=0;
  //msglog->fileTime=0;
  //msglog->fileCount=0;
  //msglog->fileOffset=0;
  //msglog->filesize=0;
  msglog->logState=nbMsgStateCreate(context);
  if(pgmState) msglog->pgmState=pgmState;
  else msglog->pgmState=msglog->logState;
  msglog->msgbuflen=0; // no data in buffer yet
  msglog->msgbuf=(unsigned char *)malloc(NB_MSG_BUF_LEN);
  //msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  msglog->msgrec=NULL;  // no record yet
  if(strcmp(linkedname,"empty")==0 && mode&NB_MSG_MODE_PRODUCER){  // PRODUCER (includes SPOKE)
    if(nbMsgLogFileCreate(context,msglog)!=0){
      nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to create file for cabal \"%s\" node %d",msglog->cabal,msglog->node);
      free(msglog->msgbuf);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    }
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
      fprintf(stderr,"nbMsgLogOpen: from cursor fileCount=%d fileOffset=%d\n",msglog->fileCount,msglog->fileOffset);
      msglog->filesize=msgcursor.fileOffset;
      msglog->state|=NB_MSG_STATE_LOGEND;
      return(msglog);
      }
    }

  sprintf(filename,"message/%s/%s/%s",cabal,nodeName,msglog->filename);
  if((msglog->file=open(filename,O_RDONLY))<0){
    fprintf(stderr,"nbMsgLogOpen: Unable to open file %s - %s\n",filename,strerror(errno));
    free(msglog->msgbuf);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);
    }

  // If not in CURSOR mode, or in cursor mode without a cursor, we need to position by state.
  fprintf(stderr,"nbMsgLogOpen: Reading header record cabal \"%s\" node %u fildes=%d file %s\n",msglog->cabal,msglog->node,msglog->file,filename);
  if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
    fprintf(stderr,"nbMsgLogOpen: Unable to read file %s - %s\n",filename,strerror(errno));
    free(msglog->msgbuf);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);
    }
  fprintf(stderr,"nbMsgLogOpen: msgbuflen=%d\n",msgbuflen);
  //msglog->filesize+=msgbuflen;
  msglog->filesize=msgbuflen;
  msglog->msgbuflen=msgbuflen;
  msglen=ntohs(*(uint16_t *)msglog->msgbuf);
  msglog->fileOffset=msglen;
  fprintf(stderr,"nbMsgLogOpen: 1 msglog->fileOffset=%d\n",msglog->fileOffset);
  msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  errStr=nbMsgHeaderExtract(msglog->msgrec,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount); 
  if(errStr){
    fprintf(stderr,"nbMsgLogOpen: Corrupted file %s - %s\n",filename,errStr);
    free(msglog->msgbuf);
    nbFree(msglog,sizeof(nbMsgLog));
    return(NULL);  
    }
  msglog->fileTime=fileTime;
  msglog->fileCount=fileCount;
  msglog->recordTime=recordTime;
  msglog->recordCount=recordCount;
  //fprintf(stderr,"nbMsgLogOpen: Extracted header time=%u count=%u fileTime=%u fileCount=%u\n",mTime,mCount,fileTime,fileCount);
  while(!nbMsgIncludesState(msglog)){
    fprintf(stderr,"nbMsgLogOpen: File %s does not include requested state\n",filename);
    close(msglog->file);
    msglog->filesize=0;
    sprintf(filename,"message/%s/%s/m%10.10u.nbm",cabal,nodeName,fileCount-1);
    if((msglog->file=open(filename,O_RDONLY))<0){
      fprintf(stderr,"nbMsgLogOpen: Unable to open file %s - %s\n",filename,strerror(errno));
      free(msglog->msgbuf);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    if((msgbuflen=read(msglog->file,msglog->msgbuf,NB_MSG_BUF_LEN))<0){
      fprintf(stderr,"nbMsgLogOpen: Unable to read file %s - %s\n",filename,strerror(errno));
      free(msglog->msgbuf);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    msglog->filesize+=msgbuflen;
    msglog->msgbuflen=msgbuflen;
    //fprintf(stderr,"msglog->msgbuflen=%d\n",msglog->msgbuflen);
    msglen=ntohs(*(uint16_t *)msglog->msgbuf);
    msglog->fileOffset=msglen;
    fprintf(stderr,"nbMsgLogOpen: 2 msglog->fileOffset=%d\n",msglog->fileOffset);
    msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
    errStr=nbMsgHeaderExtract(msglog->msgrec,node,&tranTime,&tranCount,&recordTime,&recordCount,&fileTime,&fileCount);
    if(errStr){
      fprintf(stderr,"nbMsgLogOpen: Corrupted file %s - %s\n",filename,errStr);
      free(msglog->msgbuf);
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
    fprintf(stderr,"nbMsgLogOpen: 3 msglog->fileOffset=%d\n",msglog->fileOffset);
    if((msglog->cursorFile=open(cursorFilename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))<0){ // have a cursor file
      nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to create file '%s'",cursorFilename);
      free(msglog->msgbuf);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    if(write(msglog->cursorFile,(void *)&msgcursor,sizeof(msgcursor))<0){
      nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to write cursor to file %s - %s\n",filename,strerror(errno));
      free(msglog->msgbuf);
      nbFree(msglog,sizeof(nbMsgLog));
      return(NULL);
      }
    }
  msglog->fileOffset=0; // reset for now because nbMsgLogRead will step over header 
  // Include code here to figure out what to do if the log doesn't satisfy the requested state
  // For example, if it doesn't go back far enough for a given node, we shouldn't provide any
  // messages for that node.
  sprintf(msglog->filename,"m%10.10u.nbm",msglog->fileCount);
  fprintf(stderr,"nbMsgLogOpen: Returning open message log file %s\n",filename);
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
  fprintf(stderr,"nbMsgLogCursorWrite: msgcursor.fileOffset=%u msglog->fileOffset=%d\n",msgcursor.fileOffset,msglog->fileOffset);
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

  len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
  while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
  if(len<0) nbLogMsg(context,0,'T',"nbMsgUdpRead: first recvfrom len=%d - errno=%d %s",len,errno,strerror(errno));
  while(len>0){
    if(msgTrace){
      nbLogMsg(context,0,'T',"Datagram len=%d",len);
      nbLogDump(context,buffer,len);
      nbLogMsg(context,0,'T',"nbMsgUdpRead: fileCount=%u fileOffset=%u",msgudp->fileCount,msgudp->fileOffset); 
      }
    msglen=ntohs(*(unsigned short *)msgrec);
    if(msglen+sizeof(nbMsgCursor)!=len){
      nbLogMsg(context,0,'E',"packet len=%d not the same as msglen=%u+%u",len,msglen,sizeof(nbMsgCursor));
      return;
      }
    //if(msgTrace) nbMsgPrint(stderr,msgrec);
    nbLogMsg(context,0,'T',"nbMsgUdpRead: recvfrom len=%d",len);
    nbMsgPrint(stderr,msgrec);
    state=nbMsgLogSetState(context,msglog,msgrec);
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: state=%d",state);
    if(state&NB_MSG_STATE_SEQLOW){
      //if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: Ignoring message already seen");
      nbLogMsg(context,0,'T',"nbMsgUdpRead: Ignoring message already seen");
      //return;
      }
    else if(state&NB_MSG_STATE_SEQHIGH){
      nbLogMsg(context,0,'T',"nbMsgUdpRead: UDP packet lost - reading from message log");
      if(!(msglog->state&NB_MSG_STATE_LOGEND)){
        nbLogMsg(context,0,'L',"nbMsgUdpRead: Udp packet lost when not in LOGEND state - terminating");
        exit(1);
        }
      // Read from the message log
      while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: return from nbMsgLogRead state=%d",state);
        if(state&NB_MSG_STATE_PROCESS){  // handle new message records
          if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: calling message handler\n");
          if((rc=(*msglog->handler)(context,msglog->handle,msglog->msgrec))!=0){
            nbLogMsg(context,0,'I',"UDP message handler return code=%d",rc);
            //return;  // Need to figure out if there is really anything the handler needs to communicate back
            }
          limit--;
          }
        else if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: not processing record - state=%d",state);
        }
      // drain the udp queue
      nbLogMsg(context,0,'T',"nbMsgUdpRead: flushing UDP stream - fd=%d",msglog->socket);
      while(len>0){
        len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
        while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
        }
      nbLogMsg(context,0,'T',"nbMsgUdpRead: UDP stream flushed");
      // Read the message log again
      while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND)){
        if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: return from nbMsgLogRead state=%d",state);
        if(state&NB_MSG_STATE_PROCESS){  // handle new message records
          if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: calling message handler\n");
          if((rc=(*msglog->handler)(context,msglog->handle,msglog->msgrec))!=0){
            nbLogMsg(context,0,'I',"UDP message handler return code=%d",rc);
            //return;  // Need to figure out if there is really anything the handler needs to communicate back
            }
          limit--;
          }
        else if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: not processing record - state=%d",state);
        }
      }
    else{
      if(msgudp->fileCount>msglog->fileCount){
        msglog->fileCount=msgudp->fileCount;
        msglog->filesize=msgudp->fileOffset;
        msglog->filesize-=(msgrec->len[0]<<8)+msgrec->len[1];  // adjust offset to start of first message in new file
        if(msglog->fileJumper) (*msglog->fileJumper)(context,msglog->handle,msglog->filesize);
        }
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgUdpRead: calling nbMsgCacheInsert");
      if((rc=(*msglog->handler)(context,msglog->handle,msgrec))!=0){
        nbLogMsg(context,0,'I',"UDP message handler return code=%d",rc);
        //return;
        }
      msglog->filesize=msgudp->fileOffset;  // now adjust offset to next message location
      msglog->fileOffset=msgudp->fileOffset;  // now adjust offset to next message location
      fprintf(stderr,"nbMsgUdpRead: msglog->fileOffset=%d\n",msglog->fileOffset);
      if(msglog->mode==NB_MSG_MODE_CURSOR && nbMsgLogCursorWrite(context,msglog)<0){
        nbLogMsg(context,0,'T',"Unable to update cursor for cabal '%s' node '%s' - terminating",msglog->cabal,msglog->nodeName);
        exit(1);
        }
      limit--;
      }
    if(limit<=0){ // we can't spend all our time filling the cache - need a chance to send also
      nbLogMsg(context,0,'T',"nbMsgUdpRead: hit limit of uninterrupted reads - returning to allow writes");
      return; 
      }
    len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
    while(len==-1 && errno==EINTR) len=recvfrom(msglog->socket,buffer,buflen,0,NULL,0);
    }
  if(len==-1 && errno!=EAGAIN){
    nbLogMsg(context,0,'E',"nbMsgLogUdpRead: recvfrom error - %s",strerror(errno));
    }
  }

/*
*  Start consuming messages
*/
int nbMsgLogConsume(nbCELL context,nbMsgLog *msglog,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
  char filename[128];
  int state;

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
  sprintf(filename,"message/%s/%s/s.nbm",msglog->cabal,msglog->nodeName);
  msglog->socket=nbIpGetUdpServerSocket(context,filename,0);
  if(msglog->socket<0){
    nbLogMsg(context,0,'E',"nbMsgLogConsume: Unable to open udp server socket %s",filename);
    return(-1);
    }
  msglog->handle=handle;
  msglog->handler=handler;
  nbLogMsg(context,0,'T',"nbMsgLogConsumer: set fd=%d to non-blocking",msglog->socket);
  fcntl(msglog->socket,F_SETFL,fcntl(msglog->socket,F_GETFL)|O_NONBLOCK); // make it non-blocking
  nbLogMsg(context,0,'T',"nbMsgLogConsumer: F_GETFL return=%d",fcntl(msglog->socket,F_GETFL));
  nbListenerAdd(context,msglog->socket,msglog,nbMsgUdpRead);
  nbLogMsg(context,0,'I',"Listening for UDP datagrams as %s",filename);
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

  time(&utime);
  msglog->fileTime=utime;
  msglog->fileCount++;
  if(msglog->file){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: File already open for cabal \"%s\" node %d\n",msglog->cabal,msglog->node);
    return(-1);
    }
  node=msglog->node;
  sprintf(linkname,"message/%s/%s/m.nbm",msglog->cabal,msglog->nodeName);
  linklen=readlink(linkname,linkedname,sizeof(linkedname));
  if(linklen<0){
    nbLogMsg(context,0,'E',"nbMsgLogFileCreate: Unable to read link %s - %s\n",filename,strerror(errno));
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
  sprintf(filebase,"m%10.10d.nbm",msglog->fileCount);
  sprintf(filename,"message/%s/%s/%s",msglog->cabal,msglog->nodeName,filebase);
  if((msglog->file=open(filename,O_WRONLY|O_CREAT,S_IRWXU|S_IRGRP))<0){
    fprintf(stderr,"nbMsgLogFileCreate: Unable to creat file %s\n",filename);
    return(-1);
    }
  //fprintf(stderr,"nbMsgLogFileCreate: fildes=%d %s\n",msglog->file,filename);
  if(remove(linkname)<0){
    fprintf(stderr,"nbMsgLogFileCreate: Unable to remove symbolic link %s - %s\n",linkname,strerror(errno));
    return(-1);
    }
  if(symlink(filebase,linkname)<0){
    fprintf(stderr,"nbMsgLogFileCreate: Unable to create symbolic link %s - %s\n",linkname,filename);
    return(-1);
    }
  strcpy(msglog->filename,filebase);
  if(!msglog->hdrbuf) msglog->hdrbuf=(nbMsgRec *)malloc(sizeof(nbMsgRec)+256*sizeof(nbMsgId)); // allocate header buffer
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
  msgrec->msgids=msgids;
  msglen=(char *)msgid-(char *)msgrec;
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
*  Switch message log from reading to writing (producer) mode
*
*  Returns: -1 - error, 0 - success
*/
int nbMsgLogProduce(nbCELL context,nbMsgLog *msglog,unsigned int maxfilesize){
  char *filebase="m.nbm";
  char filename[128];
  int  node;

  //fprintf(stderr,"nbMsgLogProduce: called with maxfilesize=%d\n",maxfilesize);
  node=msglog->node;
  if(!(msglog->state&NB_MSG_STATE_LOGEND)){
    fprintf(stderr,"nbMsgLogProduce: Message log not in end-of-log state - cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(-1);
    }
  sprintf(filename,"message/%s/%s/%s",msglog->cabal,msglog->nodeName,filebase);
  if(msglog->file){
    fprintf(stderr,"nbMsgLogProduce: File open - expecting closed file\n");
    return(-1);
    }
  if((msglog->file=open(filename,O_WRONLY|O_APPEND,S_IRWXU|S_IRGRP))<0){
    fprintf(stderr,"nbMsgLogProduce: Unable to append to file %s\n",filename);
    return(-1);
    }
  msglog->maxfilesize=maxfilesize;
  msglog->msgrec=(nbMsgRec *)msglog->msgbuf;
  if(msglog->mode!=NB_MSG_MODE_SPOKE){
    nbMsgUdpLocalClientSocket(msglog);
    if(msglog->socket<0){
      fprintf(stderr,"nbMsgLogProduce: Unable to open local domain socket message/%s/%s/s.nbm for UDP output\n",msglog->cabal,msglog->nodeName);
      msglog->socket=0;
      return(-1);
      }
    }
//  optlen=sizeof(int);
//  optval=512*1024;
//  if(setsockopt(msglog->socket,SOL_SOCKET,SO_RCVBUF,&optval,optlen)<0){
//    nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to setsockopt");
//    }
//  optlen=sizeof(int);
//  optval=512*1024;
//  if(setsockopt(msglog->socket,SOL_SOCKET,SO_SNDBUF,&optval,optlen)<0){
//    nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to setsockopt");
//    }
//  optlen=sizeof(int);
//  if(getsockopt(msglog->socket,SOL_SOCKET,SO_RCVBUF,&optval,&optlen)<0){
//    nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to getsockopt");
//    }
//  else nbLogMsg(context,0,'T',"nbMsgLogOpen: SO_RCVBUF=%d",optval);
//  optlen=sizeof(int);
//  if(getsockopt(msglog->socket,SOL_SOCKET,SO_SNDBUF,&optval,&optlen)<0){
//    nbLogMsg(context,0,'E',"nbMsgLogOpen: Unable to getsockopt");
//    }
//  else nbLogMsg(context,0,'T',"nbMsgLogOpen: SO_SNDBUF=%d",optval);

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
int nbMsgLogWrite(nbCELL context,nbMsgLog *msglog,int msglen){
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
  msgrec->len[1]=msglen&0xff; 
  msglog->filesize+=msglen;
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
  nbLogMsg(context,0,'T',"nbMsgLogWrite: writing message %10.10lu.%6.6u",tv.tv_sec,tv.tv_usec);
  nbMsgPrint(stderr,msgrec);
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
  // write to UPD socket
  if(msglog->socket) nbMsgUdpClientSend(context,msglog,msgudp,msglen+sizeof(nbMsgCursor)); // don't care what happens
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

  if(datalen>NB_MSG_REC_MAX-sizeof(nbMsgRec)){
    nbLogMsg(context,0,'E',"nbMsgLogWriteOriginal: Data length %u exceeds max of %u",datalen,NB_MSG_REC_MAX-sizeof(nbMsgRec));
    return(-1);
    }
  memcpy(msglog->msgbuf+sizeof(nbMsgCursor)+sizeof(nbMsgRec),text,datalen);
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
*/
int nbMsgLogWriteData(nbCELL context,nbMsgLog *msglog,void *data,unsigned short datalen){
  unsigned short msglen;
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));
  nbMsgId *msgid=&msgrec->si;
  time_t utime;
  int node=msglog->node;
 
  if(datalen>NB_MSG_REC_MAX-sizeof(nbMsgRec)){
    nbLogMsg(context,0,'E',"nbMsgLogWriteOriginal: Data length %u exceeds max of %u",datalen,NB_MSG_REC_MAX-sizeof(nbMsgRec)); 
    return(-1);
    }
  memcpy(msglog->msgbuf+sizeof(nbMsgCursor)+sizeof(nbMsgRec),data,datalen);
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
  nbMsgRec *msgrec=(nbMsgRec *)(msglog->msgbuf+sizeof(nbMsgCursor));

  nbLogMsg(context,0,'T',"nbMsgLogWriteReplica: called");
  nbMsgPrint(stderr,msgin);
  state=nbMsgLogSetState(context,msglog,msgin);
  //nbLogMsg(context,0,'T',"nbMsgLogWriteReplica: nbMsgLogSetState returned state=%d",state);
  if(state<0){
    nbLogMsg(context,0,'E',"nbMsgLogWriteReplica: Format error in cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(-1);
    }
  if(state&NB_MSG_STATE_LOG){
    msglen=ntohs(*(uint16_t *)msgin->len);
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

  nbLogMsg(context,0,'T',"nbMsgCachePublish: called with flags=%2.2x",msgsub->flags);
  //if(msgsub->flags&NB_MSG_CACHE_FLAG_MSGLOG){
  // 2010-05-06 eat - msglog could have hit end of file, so we need to check FLAG_INBUF without first checking FLAG_MSGLOG
  if(msgsub->flags&NB_MSG_CACHE_FLAG_INBUF){
    nbLogMsg(context,0,'T',"nbMsgCachePublish: calling subscription handler for messaging remaining in message log buffer");
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
      nbLogMsg(context,0,'T',"nbMsgCachePublish: nbMsgLogRead returned state=%d",state);
      if(state&NB_MSG_STATE_PROCESS){
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
    else if(*cachePtr==0x80) *cachePtr+=sizeof(nbMsgCacheFileMarker);
    // we can ignore file markers here because nbMsgCacheInsert will
    // adjust the file position in the msglog if it has to switch this
    // subscriber back to msglog mode
    else if(*cachePtr==0){
      msgrec=(nbMsgRec *)(cachePtr+1);
      if(msgrec->type!=NB_MSG_REC_TYPE_MESSAGE){
        nbLogMsg(context,0,'L',"Fatal error in message cache - invalid message record type %u - terminating",msgrec->type);
        exit(1);
        }
      if(msgsub->flags&NB_MSG_CACHE_FLAG_AGAIN) state=NB_MSG_STATE_PROCESS;
      else state=nbMsgLogSetState(context,msgsub->msglog,msgrec);
      if(state&NB_MSG_STATE_PROCESS){
        msgsub->flags&=0xff-NB_MSG_CACHE_FLAG_AGAIN;
        if((*msgsub->handler)(context,msgsub->handle,msgrec)){
          nbLogMsg(context,0,'T',"nbMsgCachePublish: calling handler");
          // 2010-05-06 eat - doesn't seem like we should set INBUF here
          //msgsub->flags|=NB_MSG_CACHE_FLAG_INBUF|NB_MSG_CACHE_FLAG_PAUSE;
          msgsub->flags|=NB_MSG_CACHE_FLAG_PAUSE|NB_MSG_CACHE_FLAG_AGAIN;
          msgsub->cachePtr=cachePtr;
          return(messages);
          }
        else messages++;
        }
      cachePtr++; // step over flag byte
      cachePtr+=(*cachePtr<<8)|*(cachePtr+1); 
      }
    else{
      nbLogMsg(context,0,'L',"Fatal error in message cache - invalid entry type %x - terminating",*cachePtr);
      exit(1);
      }
    }
  msgsub->cachePtr=cachePtr;
  nbLogMsg(context,0,'T',"nbMsgCachePublish: Messages=%d",messages);
  return(messages);
  }

/*
*  Register message cache subscriber
*/
//nbMsgCacheSubscriber *nbMsgCacheSubscribe(nbCELL context,nbMsgCache *msgcache,unsigned char *buffer,int buflen,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec)){
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
  // for now let's just read from log
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
      if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheInsert: stop>=start");
      if(*msgcache->start==0x80){   // handle file marker - update file count and offset to jump to next file
        msgcache->fileCount++;
        msgcache->fileOffset=*(uint32_t *)(msgcache->start+1);
        msgcache->start+=sizeof(nbMsgCacheFileMarker);
        nbLogMsg(context,0,'T',"nbMsgCacheStomp: filecount=%d fileOffset=%d",msgcache->fileCount,msgcache->fileOffset);
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
            msgsub->flags|=NB_MSG_CACHE_FLAG_MSGLOG;
            msgsub->cachePtr=NULL;
            msgsub->msglog->fileCount=msgcache->fileCount;    // position message log for reading
            msgsub->msglog->filesize=msgcache->fileOffset;
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
  recordCount=ntohl(*(uint32_t *)&msgid->count);
  if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheInsert: msglen=%d recordCount=%u msgcache->endCount=%u",msglen,recordCount,msgcache->endCount);
  if(recordCount!=msgcache->endCount+1){
    if(msgTrace) nbLogMsg(context,0,'T',"nbMsgCacheInsert: returning out of sequence code - recordCount=%u endCount=%u",recordCount,msgcache->endCount);
    return(1);  // out of sequence
    }
  msgcache->endCount=recordCount;
  if(msgTrace){ // (msgTrace) info
    msgid=&msgrec->si;
    recordCount=ntohl(*(uint32_t *)&msgid->count);
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
  if(msgcache->bufferStart) free(msgcache->bufferStart);
  if(msgcache->startState) nbMsgStateFree(context,msgcache->startState);
  if(msgcache->endState) nbMsgStateFree(context,msgcache->endState);
  nbFree(msgcache,sizeof(nbMsgCache));
  }

/*
*  Open a message cache and listen for UDP packets
*/ 
nbMsgCache *nbMsgCacheAlloc(nbCELL context,char *cabal,char *nodeName,int node,int size){
  nbMsgCache *msgcache;
  nbMsgLog *msglog;
  unsigned char *msgqrec;

  msgcache=(nbMsgCache *)nbAlloc(sizeof(nbMsgCache));
  memset(msgcache,0,sizeof(nbMsgCache));
  msgcache->bufferSize=size;
  msgcache->bufferStart=malloc(size);
  msgcache->bufferEnd=msgcache->bufferStart+size;
  msgqrec=msgcache->bufferStart;
  *msgqrec=0xff;  // when first flag byte is 0xff we are at the end of the cache or end of cache buffer
  msgcache->start=msgqrec;
  msgcache->end=msgqrec;
  msgcache->startState=nbMsgStateCreate(context);
  msgcache->endState=nbMsgStateCreate(context);

  msglog=nbMsgLogOpen(context,cabal,nodeName,node,"",NB_MSG_MODE_CONSUMER,msgcache->endState);
  if(!msglog){
    nbLogMsg(context,0,'E',"nbMsgCacheOpen: Unable to open message log for cabal \"%s\" node %d",cabal,node);
    nbMsgCacheFree(context,msgcache);
    return(NULL);
    }
  msgcache->msglog=msglog;
  msgcache->endCount=msglog->recordCount;
  msgcache->fileCount=msglog->fileCount;   // initialize file count and offset
  msgcache->fileOffset=msglog->filesize;
  if(nbMsgLogConsume(context,msglog,msgcache,nbMsgCacheInsert)!=0){
    nbMsgCacheFree(context,msgcache);
    }
  msglog->fileJumper=nbMsgCacheMarkFileJump; // provide method to create file markers in the message cache
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
  nbMsgCabalEnable(context,msgnode->msgcabal);
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
  nbLogMsg(context,0,'T',"nbMsgPeerNullProducer: called - returning nothing");
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

  nbLogMsg(context,0,'T',"nbMsgPeerProducer: called");
  if(!(msgcabal->mode&NB_MSG_CABAL_MODE_SERVER)){ // 
    nbLogMsg(context,0,'W',"nbMsgPeerProducer: Cabal not in server mode - not expecting request for messages - ignoring for now");
    return(0);
    }
  if(!msgnode->msgsub){
    nbLogMsg(context,0,'T',"nbMsgPeerProducer: Something funny. Called to get more data but no subscription");
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

  nbLogMsg(context,0,'T',"nbMsgPeerConsumer: called - len=%d",len);
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
  nbLogMsg(context,0,'T',"nbMsgPeerConsumer: calling nbMsgLogWriteReplica msgrec=%p",msgrec);
  state=nbMsgLogWriteReplica(context,msgnode->msgcabal->msglog,msgrec);
  if(state&NB_MSG_STATE_PROCESS){
    nbLogMsg(context,0,'T',"nbMsgPeerConsumer: calling client message handler msgnode=%p",msgnode);
    nbLogMsg(context,0,'T',"nbMsgPeerConsumer: calling client message handler msgnode->msgcabal=%p",msgnode->msgcabal);
    nbLogMsg(context,0,'T',"nbMsgPeerConsumer: calling client message handler msgnode->msgcabal->handler=%p",msgnode->msgcabal->handler);
    rc=(*msgnode->msgcabal->handler)(context,msgnode->msgcabal->handle,msgrec);
    nbLogMsg(context,0,'T',"nbMsgPeerConsumer: rc=%d from client message handler",rc);
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

  nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: called msgcabal=%p",msgcabal);
  nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: calledd msgcabal->msglog=%p",msgcabal->msglog);
  nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: calling nbMsgLogStateToRecord msgnode=%p",msgnode);
  msglen=nbMsgLogStateToRecord(context,msgcabal->msglog,msgcabal->cntlMsgBuf,NB_MSG_NODE_BUFLEN); // gen state record
  nbLogMsg(context,0,'T',"nbMsgPeerStateProducer: sending state record  msglen=%d\n",msglen);
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
  nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: calling nbMsgLogStateFromRecord");
  msgstate=nbMsgLogStateFromRecord(context,(nbMsgRec *)data);
  if(!msgstate){
    nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: Unable to get state from state record");
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgPeerStateConsumer: calling nbMsgCacheSubscribe");
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
  
  nbLogMsg(context,0,'T',"nbMsgPeerHelloProducer: called for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  nbLogMsg(context,0,'T',"nbMsgPeerHelloProducer: verify");
  if(nbPeerSend(context,peer,(char *)&msgcabal->node->msgnoderec,sizeof(nbMsgNodeRec))!=0){
    nbLogMsg(context,0,'E',"nbMsgPeerHelloProducer: Unable to send node record");
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgPeerHelloProducer: Handing off to nbMsgPeerHelloConsumer");
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
  nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: called for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  // include code here to check the node record
  // if bad return -1
  nbLogMsg(context,0,'T',"verify msgcabal=%p",msgcabal);
  nbLogMsg(context,0,'T',"verify msgcabal->mode=%p",msgcabal->mode);
  nbLogMsg(context,0,'T',"verify msgcabal->mode=%2.2.x",msgcabal->mode);
  if(msgcabal->mode&NB_MSG_CABAL_MODE_CLIENT){ // client
    nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: Handing client off to nbMsgPeerStateProducer");
    nbPeerModify(context,peer,msgnode,nbMsgPeerStateProducer,nbMsgPeerConsumer,nbMsgPeerShutdown);
    return(nbMsgPeerStateProducer(context,peer,msgnode));
    }
  else{
    nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: Handing server off to nbMsgPeerStateConsumer");
    nbPeerModify(context,peer,msgnode,nbMsgPeerNullProducer,nbMsgPeerStateConsumer,nbMsgPeerShutdown);
    }
  nbLogMsg(context,0,'T',"nbMsgPeerHelloConsumer: returning for node %s uri %s",msgnode->name,peer->tls->uriMap[peer->tls->uriIndex].uri);
  return(0);
  }

static int nbMsgCabalAcceptHelloConsumer(nbCELL context,nbPeer *peer,void *handle,void *data,int len);
/*
*  Produce hello message from peer accepting connection
*/
static int nbMsgCabalAcceptHelloProducer(nbCELL context,nbPeer *peer,void *handle){
  nbMsgCabal *msgcabal=(nbMsgCabal *)handle;

  fprintf(stderr,"hello from nbMsgCabalAcceptHelloProducer\n");
  fflush(stderr);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for cabal=%p peer=%p ",msgcabal,peer);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for peer->tls=%p",peer->tls);
  //nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: called for cabal %s uri %s",msgcabal->cabalName,peer->tls->uriMap[peer->tls->uriIndex].uri);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: calling nbPeerSend for cabal %s uri %s",msgcabal->cabalName,peer->tls->uriMap[peer->tls->uriIndex].uri);
  if(nbPeerSend(context,peer,(char *)&msgcabal->node->msgnoderec,sizeof(nbMsgNodeRec))!=0){ // send nodeID record;
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloProducer: unable to write node record - %s",strerror(errno));
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: Server handing off to nbMsgCabalAcceptHelloConsumer");
  nbPeerModify(context,peer,msgcabal,nbMsgPeerNullProducer,nbMsgCabalAcceptHelloConsumer,nbMsgPeerAcceptShutdown);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloProducer: returning");
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
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Connection %s shutting down - cabal %s node %s peer %s",peer->tls->uriMap[peer->tls->uriIndex].uri,msgcabal->cabalName,msgcabal->node->name,msgnode->name);
    return(0);
    }
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: called for cabal %s",msgcabal->cabalName);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: buffer received - len=%d",len);
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: size expecting - size=%d",sizeof(nbMsgNodeRec));
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
    nbLogMsg(context,0,'E',"nbMsgCabalAcceptHelloConsumer: Node %s is not in a disconnected state - %d - shutting down new connection",msgnode->name,msgnode->state);
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: Node %s connecting",msgnode->name);
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
  nbLogMsg(context,0,'T',"nbMsgCabalAcceptHelloConsumer: Setting connecting state");
  return(0);
  }

/*
*  Create a message cabal node
*
*  serviceName  - "peer", "server", or "client"
*
*  type - "hub", "spoke", "source", "sink"
*
*/
nbMsgNode *nbMsgNodeCreate(nbCELL context,char *cabalName,char *nodeName,nbCELL nodeContext,char *serviceName){
  nbMsgNode *msgnode;
  int        nodeNumber;
  char      *uri;
  char      *nodeType;
  int        type=0;

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
  if(strcmp(serviceName,"peer")==0) type|=NB_MSG_NODE_TYPE_SERVER|NB_MSG_NODE_TYPE_CLIENT;
  else if(strcmp(serviceName,"server")==0) type|=NB_MSG_NODE_TYPE_SERVER;
  else if(strcmp(serviceName,"client")==0) type|=NB_MSG_NODE_TYPE_CLIENT;
  else{
    nbLogMsg(context,0,'L',"Cabal \"%s\" node \"%s\" service \"%s\" not recognized",cabalName,nodeName,serviceName);
    return(NULL);
    }
  nbLogMsg(context,0,'T',"calling nbTermOptionString");
  uri=nbTermOptionString(nodeContext,serviceName,"");
  if(!*uri && type&(NB_MSG_NODE_TYPE_HUB|NB_MSG_NODE_TYPE_SOURCE)){  // hub or source need to provide uri
    nbLogMsg(context,0,'W',"Cabal \"%s\" node \"%s\" %s not defined",cabalName,nodeName,serviceName);
    return(NULL);
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
  nbLogMsg(context,0,'T',"nbMsgNodeCreate: returning %s",uri);
  return(msgnode);
  }

void nbMsgCabalPrint(FILE *file,nbMsgCabal *msgcabal){
  nbMsgNode *msgnode;
  int count=0;
  for(msgnode=msgcabal->node->next;msgnode!=msgcabal->node && count<256;msgnode=msgnode->next){
    fprintf(file,"mode=%2.2x node=%s number=%d order=%d type=%2.2x state=%2.2x downTime=%d\n",msgcabal->mode,msgnode->name,msgnode->number,msgnode->order,msgnode->type,msgnode->state,msgnode->downTime);
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
  //nbMsgNode *msgnode;
  nbCELL     cabalContext,nodeContext,tlsContext;
  //int        socket;
  //nbTLSX    *tlsx;
  char *ring;
  //char *interface;
  //int port;
  char *cursor,*delim;
  char peerName[NB_MSG_NAMESIZE];
  nbMsgNode *peerNode;
  nbMsgNode *anchor=NULL; // first in list at time self is encountered in ring
  char *serviceName[3]={"client","server","peer"};
  int serviceIndex;

  nbLogMsg(context,0,'T',"nbMsgCabalAlloc: called cabal=%s node=%s mode=%d",cabalName,nodeName,mode);
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
  msgcabal->cntlMsgBuf=(unsigned char *)malloc(NB_MSG_CABAL_BUFLEN);
  strcpy(msgcabal->cabalName,cabalName);
  msgcabal->node=nbMsgNodeCreate(context,cabalName,nodeName,nodeContext,serviceName[mode-1]);
  if(!msgcabal->node){
    nbLogMsg(context,0,'E',"nbMsgCabalAlloc: Cabal \"%s\" has no node named \"%s\"",cabalName,nodeName);
    return(NULL);
    }
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
    for(serviceIndex=0;serviceIndex<3;serviceIndex++){
      // note: we are working up to allowing a cabal service to work as both a client
      // and a server.  This means the cabal descriptions needs to tell us what
      // a given node is willing to be.  Here we are assuming we are one thing and
      // want to treat others as the opposite.
      peerNode=nbMsgNodeCreate(context,cabalName,peerName,nodeContext,serviceName[serviceIndex]); 
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
        }
      }
    msgcabal->nodeCount++;
    }
  nbMsgCabalPrint(stderr,msgcabal);
  // some types of nodes need to listen for a connection - source (server) and hub (client and server)
  if(msgcabal->node->type&(NB_MSG_NODE_TYPE_HUB|NB_MSG_NODE_TYPE_SOURCE)){ 
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

  nbLogMsg(context,0,'T',"nbMsgCabalServer: calling nbMsgCabalAlloc cabal=%s node=%s",cabalName,nodeName);
  msgcabal=nbMsgCabalAlloc(context,cabalName,nodeName,NB_MSG_CABAL_MODE_SERVER); // server mode
  nbLogMsg(context,0,'T',"nbMsgCabalServer: msgcabal->node->number=%d",msgcabal->node->number);
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

  nbLogMsg(context,0,'T',"nbMsgCabalClient: called");
  msgcabal=nbMsgCabalAlloc(context,cabalName,nodeName,NB_MSG_CABAL_MODE_CLIENT); // 1 for client
  if(!msgcabal) return(NULL);
  msgcabal->handle=handle;
  msgcabal->handler=handler;
  nbLogMsg(context,0,'T',"msgcabal->node->number=%d",msgcabal->node->number);
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
*    NB_MSG_CABAL_MODE_PEER       - Produces and consumes messages
*    NB_MSG_CABAL_MODE_PRODUCER   - Produces messages but does not consume or share others
*    NB_MSG_CABAL_MODE_SHARE      - Consumes and shares, but does not produce
*    NB_MSG_CABAL_MODE_CONSUMER   - Consumes but does not share
*
*  The goal of a consumer is to find a node other than a consumer to get messages from.
*  The goal of a share is the same as a consumer.
*  The goal of a producer is to find a peer or share to send to.
*  The goal of a peer is the sum of the above.
*  Goal
*    sink    - connect to one share or peer (in order of preference to tap into message stream)
*    source  - connect to one share or peer (in order of preference to contribute to message stream)
*    store   - connect to one share or peer (in order of preference to tap into and contribute to message stream)
*    peer      - Same as share.
*
*  A sink or store may be 0 instance because they don't contribute messages
*  A source or peer must be 1-255
*
*    NB_MSG_CABAL_MODE_CLIENT
*    NB_MSG_CABAL_MODE_SERVER
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
    nbLogMsg(context,0,'E',"Unable to alloca cabal structure");
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

  nbLogMsg(context,0,'T',"nbMsgPeerCacheMsgHandler: called");
  size=(msgrec->len[0]<<8)|msgrec->len[1];
  if(!size){
    nbLogMsg(context,0,'L',"nbMsgPeerCacheMsgHandler: we should not receive a zero length record - terminating");
    exit(1);
    }
  // ignore messages that have been at the peer node
  msgid=&msgrec->si;
  if(msgid->node==msgnode->number) return(0);
  msgid+=2;
  for(msgids=msgrec->msgids;msgids;msgids--){
    if(msgid->node==msgnode->number) return(0);
    }
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

  nbLogMsg(context,0,'T',"nbMsgCabalEnable: called for cabal %s node %s mode=%2.2x type=%2.2x",msgcabal->cabalName,msgcabal->node->name,msgcabal->mode,msgcabal->node->type);
  nbMsgCabalPrint(stderr,msgcabal);

  nbClockSetTimer(0,msgcabal->synapse); // cancel previous timer if set
  time(&utime);
  expirationTime=utime;
  expirationTime-=30;  // wait 30 seconds before connecting after a disconnect

  // Handle web topology sink (client) and source (server)
  if(msgcabal->node->type&NB_MSG_NODE_TYPE_FAN){
    if(msgcabal->node->type&NB_MSG_NODE_TYPE_SOURCE){
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: source node waits for connection from sink nodes");
      return(0);
      }
    // A sink node tries to connect to all source nodes (we don't connect to hubs---thats what spoke nodes do)
    if(msgcabal->node->type&NB_MSG_NODE_TYPE_SINK){
      nbLogMsg(context,0,'T',"nbMsgCabalEnable: sink client for cabal %s node %s",msgcabal->cabalName,msgcabal->node->name);
      for(count=0,msgnode=msgcabal->node->next;msgnode!=msgcabal->node && count<limit;msgnode=msgnode->next){
        if(msgnode->type&NB_MSG_NODE_TYPE_SOURCE && msgnode->type&NB_MSG_NODE_TYPE_SERVER){
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
        if(msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_CLIENT){
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
        if(msgnode->type&NB_MSG_NODE_TYPE_HUB && msgnode->type&NB_MSG_NODE_TYPE_SERVER){
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
  if(!preferred) nbSynapseSetTimer(context,msgcabal->synapse,15);
  return(0);
  }
