/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     nbprotocol.h 
*
* Title:    NodeBrain Protocol (NBP) Header
*
* Function:
*
*   This header defines routines that implement the NodeBrain Protocol for
*   peer-to-peer communication.  
*
* See nbprotocol.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001-02-10 Ed Trettevik (prototype version extracted from nodebrain.c)
* 2001-03-06 eat - version 0.2.4
*             1) Fixed bug in secret key translation to and from network byte
*                order.
*             2) Include checksum in authentication request packet.
* 2001-02-18 eat - version 0.3.0
*             1) Started to move the protocol code from nodebrain.c into this
*                header to better organize it.  Only the client part has been
*                completely moved.
* 2002-05-04 eat - version 0.3.2
*             1) Including support for a "skull" session.  When a skull session
*                is requested, the listener spawns a new nodebrain process to
*                handle the session.  A skull is "empty" unless the client
*                defines rules, perhaps by issuing a "load" command.
*             2) Started specification "1".  The 0.3.2 client will still
*                specifiy "0" as the required version, but indicate that "1" is
*                supported.  If the server replies with specification "1", the
*                session is upgraded.  When all servers are at 0.3.2 or later,
*                clients can request "1" in the connection request and special
*                support for version "0" can be dropped.
* 2002-05-13 eat - version 0.3.2 A4
*             1) Included prototype nbpCopy function and supporting
*                PUTFILE transaction.
* 2002-05-14 eat - version 0.3.2 A4
*             1) Included ASCII and BINARY options for nbpCopy
* 2002-11-19 eat - version 0.4.2 B2
*             1) changed NBP$* to NBP_* to compile on MVS/USS
*
* 2003-03-15 eat 0.5.1  Modified to work with new make file
* 2008-03-24 eat 0.7.0  Added includes for supporting headers removed from nb.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#include "nbske.h"
#include "nbchannel.h"
#include "nbvli.h"
#include "nbpke.h"
#include "nbprotokey.h"
#include "nbbrain.h"

extern int  nbpmin;  /* lowest supported protocol version */
extern int  nbpmax;  /* highest supported protocol version */
extern int  nbp;     /* user specified max - for testing */

extern int  skull_socket;  /* process is a skull - value is socket number */
//extern int  serving;       /* currently processing a server command */
extern char serveoar[256]; /* nb file used by the nbpServe */

/*
*  A connection request packet is the first packet presented to a peer.  This
*  packet is used for peer authentication, and establishes the protocol version
*  used for communcation.  The version number is the first byte, so nothing is
*  committed for future versions.
*
*  Under NBP1 the auth_type and auth_vers fields are SESSION[BEGIN]
*/
struct NBP_CONNECT_MSG{    /* NBP Connection Message */
  char   version;          /* NBP minimum version number - 0 or 1 */
  char   auth_type;        /* authentication type */
  char   auth_vers;        /* authentication version */
  char   client_vers;      /* client capable NBP version - 1 */
  char   auth_data;        /* authentication data - depends on auth type/version */
  };

struct NBP_MESSAGE{        /* NBP Message Structure */
  char   trancode;         /* transaction code */
  char   msgcode;          /* message code  */
  char   text[NB_BUFSIZE]; /* message text - format depends on verb */
  };

# define NBP_MSG_HALT      0  /* Halt transaction and return to READY state - except SESSION[HALT] */
# define NBP_MSG_BEGIN     1  /* Begin a transaction - normally client request to server */    
# define NBP_MSG_OK        2  /* Agree to transaction - normally server to client */
# define NBP_MSG_END       3  /* End a transaction - normally server has the last word, but not always */
# define NBP_MSG_CHALLENGE 4  /* SESSION[CHALLENGE] - server challenges client */
# define NBP_MSG_RESPONSE  5  /* SESSION[RESPONSE]  - client responds to challenge */

# define NBP_TRAN_SESSION  0  /* A session is a transaction that contains all other transactions */
                              /* The transaction may be used in mid-session to switch identities */
# define NBP_TRAN_EXECUTE  1  /* Server executes NB command supplied by client */
# define NBP_TRAN_EXECSET  2  /* Server executes a set of NB commands supplied by client */
# define NBP_TRAN_PUTFILE  3  /* Server reads data from client and writes (fputs) to a file */
# define NBP_TRAN_GETFILE  4  /* Server reads data from a file and writes to client */

/*
* The following definitions are retained for compatibility support for NBP version 0.  We don't
* need to worry about the conflicting definitions because both client and server know which version
* of the protocol is being used for a given session.  The NBP_TRAN_AR message also include the
* first command of a possible set of commands terminated by a STOP, which also terminates the
* session. 
*/
# define NBP_TRAN_AC       1  /* Authentication Challenge [0] - see SESSION[CHALLENGE] [1] */
# define NBP_TRAN_AF       2  /* Authentication Failure   [0] - see SESSION[HALT]      [1] */
# define NBP_TRAN_AR       3  /* Authentication Response  [0] - see SESSION[RESPONSE]  [1] */

/*
*  A file transfer request packet is used to transmit a file between to systems, 
*  performing a command before a file read and after a file write.  The
*  command is interpreted by the nodebrain interpreter, but system 
*  commands are the most useful for this purpose.
*
*    file <file_name> {put|get} <brain>: command
*/
struct NBP_FILE_REQ{       /* NBP File Transfer Request */
  char   request;          /* Always NBP_MSG_FW or NBP_MSG_FR */
  char   format;           /* Format number */ 
  char   command[4094];    /* Null terminated nonce
                              followed by null terminated file command
                              Syntax -          file_name : nodebrain command 
                              Write example -
    /tmp/my.tar.Z :-[username] uncompress /tmp/my.tar.Z;cd /home/me;tar -xf /tmp/my.tar;chmod ...;chown ...;
                              Read example -
    /tmp/my.tar.Z :-[username] tar -cf /tmp/my.tar /opt/sysmon/bin;compress /tmp/my.tar; */
  };

/*
*  An NBP_SESSION structure is used to share information between multiple
*  functions that manage a session.
*/

# define NBP_UNCONNECTED   0
# define NBP_CONNECTED     1
# define NBP_SERVER_AUTH   2  /* Server authenticated by client - version 0 */
# define NBP_AUTHENTICATED 3  /* Client and Server authenticated */
# define NBP_ACCEPTED      4
# define NBP_STOPPED       5
# define NBP_TERMINATED    6

# define NBP_OPT_NONE      0  /* No option */
# define NBP_OPT_CLOSE     1  /* Disconnect after send (nbpSend) */

struct NBP_SESSION{         /* NBP Peer-to-peer session */
  char   version;           /* Protocol version number */
  int    status;            /* Session status - see NBP_* above */
  int    trancode;          /* transaction code */
  int    msgcode;           /* Message code   - see NBP_MSG_* above */
  int    option;            /* Message option - see NBP_OPT_* above */
  NB_PeerKey *selfIdentity; /* Session identity - self */
  NB_PeerKey *peerIdentity; /* Session identity - peer */
  struct NB_TERM  *peer;     /* Term defining the peer brain */
  struct CHANNEL  *channel;  /* Nodebrain communication channel */
  char   client_nonce[21];  /* Client time stamp and random number */
  char   server_nonce[21];  /* Server time stamp and random number */
  char   context[256];      /* Context name for command interpretation */ 
  char   buffer[NB_BUFSIZE];      /* Session buffer */ 
  };

extern struct NBP_SESSION *currentSession;

// Old listener structure still used for transition

struct LISTENER{
  struct NB_CELL cell;       /* cell header */
                             /* The object's alert method is the protocol handler */
                             /* When the object's value is nb_Disabled the listener is disabled */
                             /* When True, the listener is enabled */
                             /* When False, the listener is enabled but ignored by select */
                             /* When Unknown, the listener will be destroyed */
  int version;               // 0 - pre 0.6.8, 1 - 0.6.8 transitional listener
  struct NB_TERM   *context;    /* parent context */
  struct NB_TERM   *term;       /* term pointing to this object - listener name */
  struct NB_TERM   *brainTerm;  /* brain listener represents */
  struct NB_TERM   *dstBrain;   /* destination brain */
  unsigned short port;       /* TCP or UDP port number, RAW protocol number */
  struct STRING *address;    /* address */
  struct NB_NBP_KEY *identity; /* identity for NBP authentication */
  int    fildes;               /* file descriptor */
#if defined(WIN32)
  WSAEVENT hEvent;             // Windows socket event handle
#endif
  void *session;             /* listener type specific session handle */
  };

extern struct LISTENER *listenerFree; /* free listener list */

/* functions */

int nbpMsg(struct NBP_SESSION *session,char trancode,char msgcode,char *text,size_t len);
struct NBP_SESSION *nbpOpen(char nbp,NB_Term *peer,char *context);
int nbpClose(struct NBP_SESSION *session);
int nbpPut(struct NBP_SESSION *session,char *command); 
void nbpStop(struct NBP_SESSION *session);

int nbpBegin(struct NBP_SESSION *session,char trancode,char *text);
int nbpEnd(struct NBP_SESSION *session,char trancode);
struct NBP_SESSION *nbpOpenTran(char nbp,NB_Term *brainTerm,char trancode,char *text);
int nbpCloseTran(struct NBP_SESSION *session,char trancode);

int nbpServeAuth(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf,int len);
void nbpServeExecute(struct LISTENER *ear,struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf);
void nbpServePutFile(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf);
void nbpServeGetFile(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf);
void nbpServeSession(struct LISTENER *ear,struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf,int len);

#if defined(WIN32)
void CALLBACK winAlarmTimeout();
#endif

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbpServe(struct LISTENER *ear,struct NBP_SESSION *session,int nbp,char *oar);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbpSend(int nbp,struct NB_TERM *brainTerm,char *context,char *command);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbpCopy(int nbp,NB_Term *srcBrainTerm,char *srcFile,NB_Term *dstBrainTerm,char *dstFile,char mode);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct NBP_SESSION *nbpNewSessionHandle(NB_PeerKey *identity);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbpFreeSessionHandle(struct NBP_SESSION *session);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct LISTENER *nbpListenerNew(NB_Cell *context);

int nbqGetDir(char *qname,NB_Term *brainTerm);

void nbqSend(NB_Term *brainTerm);

int nbqStoreCmd(NB_Term *brainTerm,char *cursor);

