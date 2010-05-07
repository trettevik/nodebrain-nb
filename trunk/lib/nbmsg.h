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
* File:     nbmsg.h
*
* Title:    Message API Header
*
* Function:
*
*   This header defines routines that implement the NodeBrain Message API. 
*
* See nbmsg.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2009/12/12 eat 0.7.7  (original prototype) 
*=============================================================================
*/
#ifndef _NBMSG_H_
#define _NBMSG_H_       /* never again */

#include <sys/un.h>

//==================================================
// Message Log Structures
//==================================================

#define NB_MSG_NAMESIZE 32

#define NB_MSG_NODE_MAX 256     // maximum number of nodes
#define NB_MSG_REC_MAX 64*1024  // maximum msg record length
#define NB_MSG_BUF_LEN 64*1024  // maximum msg record length

typedef struct NB_MSG_ID{   // Message Id in binary - network byte order
  unsigned char node;       // node number 0 to 255
  unsigned char time[4];    // UTC time
  unsigned char count[4];   // wrap around counter
  } nbMsgId;

typedef struct NB_MSG_REC{  // Message record in binary - network byte order
  unsigned char len[2];     // record length 13 byte to 64KB
  unsigned char type;       // record type - see NB_MSG_REC_TYPE_*
  unsigned char datatype;   // type of data  - see NB_MSG_REC_DATA_*
  unsigned char msgids;     // number of additional msgid values (state or path)
  nbMsgId       si;         // state msgid - assigned by original node
  nbMsgId       pi;         // path msgid - assigned by local node
  } nbMsgRec;

#define NB_MSG_REC_TYPE_STATE      0   // state record - path provides state - no data
#define NB_MSG_REC_TYPE_HEADER     1   // header - list state, but data provides link to prior file
#define NB_MSG_REC_TYPE_MESSAGE    2   // standard message
// The idea of an express message is one that is accepted out of sequence
// via the unix domain UDP port but doesn't change the state
#define NB_MSG_REC_TYPE_NOOP       4   // no operation - used to disable message without impacting counter checks
#define NB_MSG_REC_TYPE_FOOTER   255   // last record in a file

#define NB_MSG_REC_DATA_NONE 0  // no data
#define NB_MSG_REC_DATA_CHAR 1  // character data
#define NB_MSG_REC_DATA_BIN  2  // binary data
#define NB_MSG_REC_DATA_ID   3  // nbMsgId structure (used on header and footer for file id

typedef struct NB_MSG_NUMBER{
  uint32_t time;
  uint32_t count;
  } nbMsgNum;

typedef struct NB_MSG_STATE{
  nbMsgNum msgnum[NB_MSG_NODE_MAX];
  } nbMsgState;

typedef struct NB_MSG_UDP_PREFIX{      // Unix domain UDP packet prefix
  uint32_t fileCount;                  // number of message log file where this message was written
  uint32_t fileOffset;                 // offset in message log file where this message was written
  } nbMsgUdpPrefix;

typedef struct NB_MSG_LOG{
  char cabal[32];                      // name of message cabal - group of nodes
  char nodeName[32];                   // name of node within cabal
  int  node;                           // node number 0-255
  char filename[32];                   // base file name
  int  mode;                           // requested mode
  int  state;                          // state used to control operation sequence
  int  file;
  int  socket;                         // socket for UDP communication between producer and consumer
  struct sockaddr_un un_addr;          // unix domain socket address
  uint32_t filesize;                   // file position (size when writing)
  uint32_t maxfilesize;                // maximum filesize - new message log file started at this size
  uint32_t fileTime;                   // starting time of current file 
  uint32_t fileCount;                  // log file number 
  uint32_t recordTime;                 // log record time
  uint32_t recordCount;                // log record number
  nbMsgState *logState;
  nbMsgState *pgmState;
  int         msgbuflen;               // length of used portion of msgbuf (for reading)
  nbMsgRec *hdrbuf;                    // buffer for file header record
  unsigned char *msgbuf;               // message buffer - size defined by NB_MSG_BUF_LEN
  nbMsgRec *msgrec;                    // pointer within msgbuf when reading (same as msgbuf for writing)
  void     *handle;                    // handle and handler when in "accept" mode 
  int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec);
  } nbMsgLog;

#define NB_MSG_MODE_CONSUMER  0        // may call nbMsgLogConsume after open
#define NB_MSG_MODE_SINGLE    1        // single file reader
#define NB_MSG_MODE_PRODUCER  2        // may call nbMsgLogProduce after reading to end of log

#define NB_MSG_STATE_INITIAL  0        // initial start of msglog structure
#define NB_MSG_STATE_PROCESS  1        // program needs to process the last record
#define NB_MSG_STATE_LOG      2        // last record needs to be logged
#define NB_MSG_STATE_SEQLOW   4        // record has low sequence number  < +1
#define NB_MSG_STATE_SEQHIGH  8        // record has high sequence number > +1
#define NB_MSG_STATE_FILEND  16        // end of individual log file - nbMsgRead will skip to next on next call
#define NB_MSG_STATE_LOGEND  32        // end of log - nbMsgRead will reopen active file and seek on next call
                                       // nbMsgProduce may be called in this state
#define NB_MSG_STATE_ERROR   -1        // all bits on

int nbMsgLogSetState(nbCELL context,nbMsgLog *msglog,nbMsgRec *msgrec);
int nbMsgLogWrite(nbCELL context,nbMsgLog *msglog,int msglen);
int nbMsgLogFileCreate(nbCELL context,nbMsgLog *msglog);

// Message Log API

extern int nbMsgLogStateToRecord(nbCELL context,nbMsgLog *msglog,unsigned char *buffer,int buflen);
extern nbMsgState *nbMsgLogStateFromRecord(nbCELL context,nbMsgRec *msgrec);

extern void *nbMsgData(nbCELL context,nbMsgRec *msgrec,int *datalen);

extern nbMsgState *nbMsgStateCreate(nbCELL context);
extern int nbMsgStateSet(nbMsgState *state,int node,uint32_t time,uint32_t count);

extern nbMsgLog *nbMsgLogOpen(nbCELL context,char *cabal,char *nodeName,int node,char *filename,int mode,nbMsgState *pgmState);
extern int nbMsgLogClose(nbCELL context,nbMsgLog *msglog);
extern int nbMsgLogRead(nbCELL context,nbMsgLog *msglog);
extern int nbMsgLogConsume(nbCELL context,nbMsgLog *msglog,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec));

extern int nbMsgLogProduce(nbCELL context,nbMsgLog *msglog,unsigned int maxfileSize);
extern int nbMsgLogWriteString(nbCELL context,nbMsgLog *msglog,unsigned char *text);
extern int nbMsgLogWriteData(nbCELL context,nbMsgLog *msglog,void *data,unsigned short datalen);
extern int nbMsgLogWriteReplica(nbCELL context,nbMsgLog *msglog,nbMsgRec *msgrec);

//==================================================
// Message Cache Structures
//==================================================

// Records in the message cache have a one byte prefix that identifies the type

// 0x00 - message     - followed by nbMsgRec structure
// 0x80 - file marker - followed by 4 byte file position and fileCount increment is implied
// 0xff - stop        - marks end of cache entries
//                      if not pointed to by end pointer, indicates a wrap to front of cache buffer

typedef struct NB_MSG_CACHE_FILE_MARKER{  // file marker in cache enables maintenance of msglog fileCount
                                          // and filesize across file boundaries.  Message record lengths
                                          // are used to update filePos between markers.
  unsigned char code;        // always x80
  unsigned char filePos[4];  // host byte order file position (unaligned uint32_t)
  } nbMsgCacheFileMarker;

typedef struct NB_MSG_CACHE_SUBSCRIBER{
  struct NB_MSG_CACHE_SUBSCRIBER *next;
  struct NB_MSG_CACHE *msgcache;  // owning message cache
  unsigned char  flags;           // flag bits to coordinate operations
  unsigned char *cachePtr;        // pointer to next message in the cache
                                  // (only valid if NB_MSG_CACH_FLAG_MSGLOG is off)
  nbMsgLog      *msglog;          // message log, includes msgstate and buffer for reading
                                  // we have to maintain the msglog->fileCount and msglog->filesize
                                  // fields even when reading from the cache
//  unsigned char *buffer;          // subscriber's 64KB buffer first 2 bytes give used length
  int            buflen;          // length of buffer (64KB recommended)
  void          *handle;          // Handle used by subscriber (e.g. nbMsgCabal)
  int          (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec);  // subscriber's message handler
  } nbMsgCacheSubscriber;

#define NB_MSG_CACHE_FLAG_PAUSE   1 // subscriber is not ready - wait for subscriber to call nbMsgCachePublish 
#define NB_MSG_CACHE_FLAG_MSGLOG  2 // reading from message log
#define NB_MSG_CACHE_FLAG_INBUF   4 // next message is in the msglog buffer
#define NB_MSG_CACHE_FLAG_AGAIN   8 // next message in cache should be processed

typedef struct NB_MSG_CACHE{      // message cache structure
  nbMsgCacheSubscriber *msgsub;   // list of subscribers
  int            bufferSize;
  unsigned char *bufferStart;
  unsigned char *bufferEnd;
  unsigned char *start;
  unsigned char *end;
  nbMsgState    *startState;
  nbMsgState    *endState;
  nbMsgLog      *msglog;
  uint32_t      endCount;         // message count of last message in cache
  } nbMsgCache;

// Message Cache API

extern nbMsgCache *nbMsgCacheAlloc(nbCELL context,char *cabal,char *nodeName,int node,int size);
//extern nbMsgCacheSubscriber *nbMsgCacheSubscribe(nbCELL context,nbMsgCache *msgcache,unsigned char *buffer,int buflen,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec));
extern nbMsgCacheSubscriber *nbMsgCacheSubscribe(nbCELL context,nbMsgCache *msgcache,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec));
extern int nbMsgCacheCancel(nbCELL context,nbMsgCacheSubscriber *msgsub);
int nbMsgCachePublish(nbCELL context,nbMsgCacheSubscriber *msgsub);

//==================================================
// Message Peer Structures
//==================================================

typedef struct NB_MSG_NODE_REC{
  char               name[NB_MSG_NAMESIZE];
  // will add secret here
  } nbMsgNodeRec;

typedef struct NB_MSG_NODE{
  struct NB_MSG_NODE *prior;
  struct NB_MSG_NODE *next;
  struct NB_MSG_CABAL *msgcabal;
  char                name[NB_MSG_NAMESIZE];
  int                 number;      // -1 for nodes without numbers
  unsigned char       type;        // Type of node --- see NB_MSG_NODE_TYPE_*
  unsigned char       state;
  unsigned char       order;
  int                 downTime;    // time of last disconnect
  char               *dn;
  nbPeer             *peer4Connect;// medel peer for initiating a connection
  nbPeer             *peer;        // peer connected by either party       
  nbMsgCacheSubscriber *msgsub;    // message cache subscriber 
  nbMsgState         *msgstate;    // state vector for server nodes (NULL for root node)
  nbMsgNodeRec        msgnoderec;
  } nbMsgNode;

#define NB_MSG_NODE_STATE_DISCONNECTED  0  // node is not connected to peer
#define NB_MSG_NODE_STATE_CONNECTING    1  // node in the process of establising a connection
#define NB_MSG_NODE_STATE_CONNECTED     2  // node's peer connection has been established

#define NB_MSG_NODE_TYPE_CLIENT  1 // Consumes messages
#define NB_MSG_NODE_TYPE_SERVER  2 // Serves messages
#define NB_MSG_NODE_TYPE_HUB     4 // Responsible for self only. Otherwise HUB - special responsibility
// not using the next two yet
#define NB_MSG_NODE_TYPE_SINK    8 // Consumes but doesn't produce or share
#define NB_MSG_NODE_TYPE_SOURCE 16 // Produces but doesn't consume

#define NB_MSG_NODE_BUFLEN 64*1024  // 64K buffer

typedef struct NB_MSG_CABAL{
  int         mode;                // Operating mode flags - see NB_MSG_CABAL_MODE_*
  nbPeer     *peer;                // handle for peer API listener - use root node when we switch to single port
  char        cabalName[NB_MSG_NAMESIZE];
  int         nodeCount;           // number of nodes other than the root node
  nbMsgNode  *node;                // root is self
  nbMsgCache *msgcache;            // pointer to message cache structure used by a message server 
  nbMsgLog   *msglog;              // used by peers in client mode
  void       *handle;              // handle provided by nbMsgCabalClient for handler
  int       (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec); 
  unsigned char *cntlMsgBuf;       // control message buffer
  nbCELL      synapse;             // synapse cell for setting medulla timer
  } nbMsgCabal;

// The mode is used both as an index and a bit mask
// If more bits are needed, we have to stop using it as an index
#define NB_MSG_CABAL_MODE_CLIENT  1  // Requests messages from servers
#define NB_MSG_CABAL_MODE_SERVER  2  // Serves messages up for clients
#define NB_MSG_CABAL_MODE_PEER    3  // Both a client and server

#define NB_MSG_CABAL_BUFLEN 16*1024  // 16K buffer for control message (state, etc.)

// Message Peer API

extern nbMsgCabal *nbMsgCabalOpen(nbCELL context,int mode,char *cabalName,char *nodeName,nbMsgState *msgstate,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec));
extern nbMsgCabal *nbMsgCabalServer(nbCELL context,char *cabal,char *nodeName);
extern nbMsgCabal *nbMsgCabalClient(nbCELL context,char *cabal,char *nodeName,void *handle,int (*handler)(nbCELL context,void *handle,nbMsgRec *msgrec));
extern int nbMsgCabalClientSync(nbCELL context,nbMsgCabal *msgcabal,nbMsgState *msgstate);
extern int nbMsgCabalEnable(nbCELL context,nbMsgCabal *msgcabal);
extern int nbMsgCabalDisable(nbCELL context,nbMsgCabal *msgcabal);
extern int nbMsgCabalFree(nbCELL context,nbMsgCabal *msgcabal);

#endif
