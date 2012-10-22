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
* File:     nbprotocol.c 
*
* Title:    NodeBrain Protocol (NBP) Routines [prototype]
*
* Function:
*
*   This header provides routines that implement the NodeBrain Protocol for
*   brain-to-brain communication.  This is a high level application layer
*   implemented on top of a lower level layer called "NodeBrain channels"
*   which uses standard TCP/IP socket connections.
*
*      NBP       nbprotocol.h    authentication and request processing
*      NBC       nbchannel.h     data send/recv and encryption     
*      TCP/IP                    data packets
*
* NOTE:
*
*   2002/02/17 - The descriptions here have never been implemented and will most
*   likely be reinvented as we evolve this layer into a useful set of higher
*   level functions.  The documentation has been retained to preserve the 
*   general concepts.  This header does implement an authentication and
*   encryption layer appropriate for use at this level.
*
* Synopsis:
*
*   #include "nb.h"
*
*   struct NBP_SESSION *nbpNewSessionHandle(NB_PeerKey *identity);
*
*   int nbpFreeSessionHandle(struct NBP_SESSION *session);
*
*   struct NBP_SESSION *nbpOpen(
*     int nbp,
*     NB_Term *brain,
*     char *context)
*
*   int nbpClose(struct NBP_SESSION *session);
*
*   int nbpPut(struct NBP_SESSION *session,command);
*
*   int nbpSend(
*     int nbp;
*     NB_Term *brainTerm,
*     char *context,
*     char *command);
*
*   int nbpCopy(nbp,srcBrainTerm,srcFile,dstBrainTerm,dstFile,mode)
*     int nbp;
*     NB_Term *srcBrainTerm;
*     char *srcFile;
*     NB_Term *dstBrainTerm;
*     char *dstFile;{
*
*   int nbpServe(ear,session,nbp,oar);       - current 
*
*   int nbpServe(                    - future 
*     struct NBP_SESSION *session,
*     NB_Term *context,
*     NB_Term *brainTerm,
*     int *(timerCallBack),
*     int *(commandCallBack) );
*
* Description
*
* NOTE:
*
*   2002/02/17 - This description needs to be updated.  It has been retained
*   to preserve the general concepts only.
*
*   These routines are used to implement NodeBrain server and client communication. 
*
*   A client communicates with a server using the following sequence of calls.
*
*      comOpen(com,brain);
*      comPut(com,command);
*      comStop(com);
*      while(comGet(com,response)){
*        ... handle response ...
*        }
*      comClose(com);
*
*   The comServe routine waits for connections on the port associated with the
*   specified brain.  The timerCallBack routine is called to get a wait time.
*   It the timer expires before a connection is made, the timerCallBack routine
*   is called again to get another wait time.  When a connection is made by a
*   client, the comServe routine calls the commandCallBack routine with any
*   commands accepted from the client.
*
*   The comServe routine is responsible for client authentication and data
*   encryption.
*
*   The commandCallBack routine is passed a COM pointer, and a client identity
*   with each command.  The command interpreter may communicate with the client
*   via comPut and comGet.  When the commandCallBack routine returns to
*   comServe, a comClose is issued by comServe. 
*   
*
*   
* Exit Codes:
*
*   0 - Successful completion
*   1 - Error (see message)
* 
*=============================================================================
* Specification Notes:
*
*  NBP0:
*
*    o  The first command of a session is passed with the
*       client's authentication response.
*    o  Each subsequent command is passed in the raw,
*       instead of as a message.
*    o  A stop terminates the message set AND the session.
*
*    NOTE: The inclusion of the first command in the final authentication has
*          two disadvantages motivating a change.
*
*          1) The generation of every request message would be complicated by
*             a need to insert authentication data in the first message of a
*             session.
*          2) Code interpreting the response to every type of request message
*             would be complicated by the possibility of an authentication
*             failure.
*    NOTE: The original design was selected to reduce the number of packets
*          exchange in short sessions.  In NBP1 we are attempting to simplify
*          longer sessions at some cost to the single command sessions.
*
*  NBP1:
*
*    o  This version breaks from the NBP0 model where each session
*       was a logical transaction between brains.  In NBP1 we allow
*       multiple transactions in a single session.  A transaction
*       starts from a READY state, where the client is expected
*       to send the first message.  A transaction always returns to
*       a READY state at completion or interruption.  Simple transactions
*       are made up of a message from client to server followed by a
*       message from server to client.  More complex transactions start
*       out like a simple transaction to arrive at an OK state
*       where both client and server know what is expected next, and
*       raw data flows in one or both directions.  Raw data is terminated
*       with a STOP (zero length packet).
*
*    o  A server provides a response to an authentication
*       sequence before any other messages are exchanged. The
*       authentication sequence is a logical transaction.
*
*           SESSION[BEGIN](authchallenge,version) ->
*           <- SESSION[CHALLENGE](accept,version,authresponse,authchallenge)
*           SESSION[RESPONSE](accept,authresponse) ->
*           <- SESSION[OK]
*
*               ... transactions ...
*
*           SESSION[HALT] ->
*           <- SESSION[END]
*
*    o  Interpreter commands may be individual messages.
*
*           EXECUTE[BEGIN](command) ->
*           <-{output}*,<-STOP,<-EXECUTE[END](completioncode)
*
*    o  Interpreter commands may be sent as a negotiated set.
*       The completion of a command set does not terminate
*       a session.
*
*           EXECSET[BEGIN](context=contextname) ->
*           <- EXECSET[OK]
*           {text ->,<-{output}*,<-STOP,<-EXECUTE[END](completioncode)}*
*           STOP ->
*           <- EXECSET[END](completioncode)
*
*    o  Data transmission is also supported.
*
*           DATASET[BEGIN](monitor,File=filename) ->
*           <- DATASET[OK]
*           {{data}*->,STOP->}
*           <- DATASET[END](completioncode)
*
*  Compatibility:
* 
*    Clients and servers negotiate and communicate using the lower of their
*    highest supported version.  They will refuse to communicate if one's lowest
*    supported version is higher than the other's highest supported version.
*    
*    o  A 0.3.2 server will support a client using NBP0 or NBP1.
*    o  A 0.3.2 client will work with a server supporting NBP0 or NBP1.
*
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
*
* 2002-10-01 eat - version 0.4.1 A6
*             1) We have introduced the notion of listener objects.  Methods
*                have been included at the bottom of this file to support NBP
*                listeners.  This general scheme allows for multiple NBP listeners
*                (i.e. multiple TCP ports) as well as FIFO, NBQ and other types
*                of listeners.   See nblistener.c for more info.
* 2002-11-14 eat - version 0.4.2 B1
*             1) Revised options for transmitting files to queues.
* 2002-11-19 eat - version 0.4.2 B2
*             1) Changed NBP$* to NBP_* to compile MVS/USS
* 2003-01-18 eat - version 0.4.4 B2
*             1) Modified nbqServePutFile() to use nbQueueGetFile() and the existing
*                call to nbQueueCommit().  This simplifies the processing of a
*                message queue by non-nb programs (like Perl scripts) because
*                unfinished files are recognized by the "%" in the second to
*                last character and don't need to be locked.
* 2005-12-23 eat 0.6.4  Included calls to medulla interface in Enable/Disable methods
*            The long range plan is to convert this listener into a skill module
*            and deprecate the old listener concept.
* 2008-03-07 eat 0.6.9  Included support for portrayed identity override
* 2008-03-25 eat 0.7.0  Upgraded bootstrap and minimum version to 1
* 2008-11-11 eat 0.7.3  Changed failure exit code to NB_EXITCODE_FAIL
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-31 dtl 0.8.7  Checker updates
* 2012-02-09 eat 0.8.7  Repaired comAuthDecode 
* 2012-02-09 eat 0.8.7  Reviewed Checker
* 2012-06-16 eat 0.8.10 Replaced rand with random
* 2012-08-31 dtl 0.8.12 Checker updates
* 2012-10-13 eat 0.8.12 Replace malloc/free with nbAlloc/nbFree
* 2012-10-13 eat 0.8.12 Checker updates
* 2012-10-16 eat 0.8.12 Replaced random with nbRandom
* 2012-10-17 eat 0.8.12 Replaced padding loops with RAND_bytes
* 2012-10-17 eat 0.8.12 Checker updates
*=============================================================================
*/
#include <openssl/rand.h>
#include "nbi.h"
#include "nbprotocol.h"
#include "nb_peer.h"
#include "nbrand.h"

int trace=0;             // trace option
//int  nbpmin=0;           /* lowest supported protocol version */
int  nbpmin=1;           /* lowest supported protocol version */
int  nbpmax=1;           /* highest supported protocol version */
int  nbp=1;              /* user specified max - for testing */
int  skull_socket=0;     /* process is a skull - value is socket number */
//int  serving=0;          /* currently processing a server command */
char serveoar[256];      /* nb file used by the nbpServe */

//int  nbpboot=0;          /* connection bootstrap protocol version */
int  nbpboot=1;          /* connection bootstrap protocol version */

struct NBP_SESSION *currentSession;  /* session pointer  */
skeKEY enKey,deKey;           /* Rijdael expanded cipher key */
skeKEY clientEnKey,clientDeKey;

struct LISTENER *listenerFree=NULL; /* free listener list */


// This is a temporary replacement for the global trace option
// in the NodeBrain library.  Required since we moved the protocol
// out of the library.
//static void nbpSetTrace(int value){
//  trace=value;
//  }

//  nbLogHandlerAdd(context,session,nbpClientOut);

//static void nbpClientOut(nbCELL context,struct NBP_SESSION session,char *buffer){
static void nbpClientOut(nbCELL context,void *session,char *buffer){
  chput(currentSession->channel,buffer,strlen(buffer));
  }

/*============================================================================
* Code transitioning from old LISTENER
*  
*/ 
// Listener constructor for skill modules - transitional
//   Some listeners have been implemented with internal skill modules
//   and share code with the deprecated listener support.  Once we have
//   released a version supporting the skill module syntax and users
//   have had an opportunity to convert, support for listeners will
//   be dropped in a subsequent release.  At that point, the LISTENER
//   structure and supporting code will go away.  The nbListenerAdd()
//   interface will be replaced by a direct nbMedulla interface.
//
struct LISTENER *nbpListenerNew(nbCELL context){
  struct LISTENER *ear;
  //ear=nbCellNew(listenerTypeNbp,&listenerFree,sizeof(struct LISTENER));
  if((ear=listenerFree)==NULL) ear=nbAlloc(sizeof(struct LISTENER));
  else listenerFree=(struct LISTENER *)ear->cell.object.next;
  ear->cell.object.value=(NB_Object *)NB_CELL_DISABLED;
  ear->version=1;                   // listener for transitioning to nodes
  ear->brainTerm=NULL;
  ear->dstBrain=NULL;               // destination brain
  ear->context=(NB_Term *)context;
  ear->term=(NB_Term *)context;
  ear->fildes=0;                    // clear file descriptor
  ear->port=0;
  ear->address=(struct STRING *)nbCellCreateString(NULL,"");
  ear->identity=NULL;
  ear->session=NULL;
  return(ear);
  }

void nbpSessionAlert(nbCELL context,int socket,void *handle){
  struct LISTENER *ear=(struct LISTENER *)handle;
  struct NBP_MESSAGE *msgbuf;
  int len;
  struct CHANNEL *channel;
  char *buffer;
  nbIDENTITY clientIdentitySave;
  nbCELL contextSave;

#if defined(WIN32)
  int mode=0;
#endif

  // Set up client environment and alert listener

  /* use global session variable so output goes to the correct client */
  currentSession=(struct NBP_SESSION *)ear->session;
  buffer=currentSession->buffer;
  msgbuf=(struct NBP_MESSAGE *)buffer;
  channel=currentSession->channel;

  len=chget(channel,buffer,NB_BUFSIZE);                /* READY state get */
  if(len>0){
    contextSave=nbSetContext((nbCELL)ear->context); // switch to client context
    clientIdentitySave=nbIdentitySetActive(context,currentSession->peerIdentity->identity);
    if(*channel->unaddr!=0) nbLogMsg(context,0,'I',"%s@%s",currentSession->peerIdentity->identity->name->value,channel->unaddr);
    else nbLogMsg(context,0,'I',"%s@%s:%u",currentSession->peerIdentity->identity->name->value,channel->ipaddr,channel->port);
    switch(msgbuf->trancode){
      case NBP_TRAN_EXECUTE:   nbpServeExecute(ear,currentSession,msgbuf); break;
      case NBP_TRAN_PUTFILE:   nbpServePutFile(currentSession,msgbuf); break;
      case NBP_TRAN_GETFILE:   nbpServeGetFile(currentSession,msgbuf); break;
      }
//    nbLogFlush(NULL);
	nbLogFlush(context);
    nbIdentitySetActive(context,clientIdentitySave);
	nbSetContext(contextSave);               // restore context
#if defined(WIN32)
    mode=1;
    ioctlsocket((SOCKET)ear->fildes,FIONBIO,&mode);
    WSAEventSelect((SOCKET)ear->fildes,ear->hEvent,FD_READ);
#endif
    return;
    }
  if(len<0) nbLogMsgI(0,'L',"nbpSessionAlert: chget failed.");
  nbpClose(currentSession);
  nbpFreeSessionHandle(currentSession);
  nbListenerRemove((nbCELL)ear->context,socket);
  // replace with call to function to give up stuff we are pointing to 
  ear->cell.object.next=(NB_Object *)listenerFree; /* insert in free list   */
  listenerFree=ear;
  }

/*
*  This is an experimental routine for cloning a listener that
*  uses new file descriptors for each "session"; e.g., socket connections.
*  When a declared listener is alerted to start a new session, it
*  can clone itself under a new fildes to enable a session to run
*  "concurrently" with other sessions and other listeners without the
*  use of multiple threads or processes.  We are relying on SELECT()
*  for this type of concurrent sessions.
*/
//struct LISTENER *nbpCloneListener(struct LISTENER *parent,struct TYPE *type,int fildes,void *session){
struct LISTENER *nbpCloneListener(struct LISTENER *parent,int fildes,void *session){
  struct LISTENER *ear;
  //ear=nbCellNew(type,&listenerFree,sizeof(struct LISTENER));
  if((ear=listenerFree)==NULL) ear=nbAlloc(sizeof(struct LISTENER));
  else listenerFree=(struct LISTENER *)ear->cell.object.next;
  //we don't put on the used list because we are managing it under the new listener scheme
  //ear->cell.object.next=(NB_Object *)listenerUsed;
  //listenerUsed=ear;
  //ear->cell.object.value=NB_OBJECT_TRUE;  /* clones are enabled by default */
  ear->cell.object.value=(NB_Object *)NB_CELL_TRUE;  /* clones are enabled by default */
  ear->brainTerm=parent->brainTerm;
  ear->dstBrain=parent->dstBrain;
  ear->context=parent->context;
  ear->term=parent->term;
  ear->fildes=fildes;       /* set file descriptor */
  ear->port=parent->port;
  ear->address=parent->address;
  ear->identity=parent->identity;
  ear->session=session;     /* set session handle */
  nbListenerAdd((nbCELL)ear->context,fildes,ear,nbpSessionAlert);
  return(ear);
  }

//============================================================================

/*
*  Alarm handler for chget timeout
*/
void alarmTimeout(sig) int sig;{
  nbLogMsgI(0,'T',"Timed out waiting for input from client.");
  nbLogFlush(NULL);
  chclose(currentSession->channel);
  nbLogMsgI(0,'T',"Channel closed.");
  }
#if defined(WIN32)
void CALLBACK winAlarmTimeout(
  HWND hwnd,         /* handle to window - none */
  UINT uMsg,         /* WM_TIMER message */
  UINT_PTR idEvent,  /* timer identifier */
  DWORD dwTime){     /* current system time */

  alarmTimeout(0);
  }
#endif


/*============================================================================
*  Routines to handle authentication block
*
*       [private_text][secret_text]
*  
*       private_text - public key encrypted secret key (decrypted with private key)
*
*                      [len][pke_block1][pke_block2]...[pke_blockN]   (see nbpke.h)
*
*       secret_text  - secret key encrypted text 
* 
*                      [ske_block1][ske_block2]...[ske_blockM]        (see nbske.h)
*
*  An authentication block is sent from one nodebrain to another as a challenge.
*  Suppose NB1 is challenging NB2.  NB1 generates a random secret key and encrypts
*  it using NB2's public key.  NB1 uses the secret key to encrypt a password
*  and its own identity name.  
*
*  The password is a time stamp and random number.  The time stamp is used
*  to ensure the password will not repeat for a very long time, and to
*  enable a time check to limit playback and ensure clocks are in sync.  The
*  random number is included to make sure the password is not predictable.
*
*  NB2 must use its private key to decrypt the secret key, and then the secret key
*  to decrypt the secret text.  Nb2 looks up NB1's public key using the identity
*  provided and generates an authentication block just like NB1 did, except NB1's
*  password is used in place of NB2's identity.
* 
*  NB1 receives NB2's response challenge.  NB1 must follow the same process of
*  decoding the challenge from NB2.  If the password returned by NB1 is
*  correct, NB2 has authenticated NB1.  NB1 then returns the token received from
*  NB2.  If it is correct, NB2 has authenticated NB1.
*  
*  In this process, NB1 and NB2 have exchanged randomly generated secret keys.
*  Each has told the other, "Use this key for the remainder of this conversation."
*  Once the secret key is received, all packets sent to the peer are encrypted
*  using the secret key.  This includes the return of the password, ensuring that
*  the secret key has actually been decoded using the peers private key.
*    
*==============================================================================*/
/*
*  Encode an authentication block and return the length.
*/
unsigned int comAuthEncode(
  unsigned char *buffer,       /* Authentication packet buffer to encode */
  unsigned int  cipher[4],     /* Secret CBC key - generated */
  skeKEY *key,                 /* Secret Rijndael key - generated */
  unsigned char timeStamp[21], /* Time stamp - generated */ 
  unsigned char *text,        /* (request) client identity name, (reply) client time stamp */ 
  NB_PeerKey *identity,   /* For public key encryption of generated secret key */
  unsigned char keylen){      /* 1 (32-bit), 2 (64-bit), ... */
   
  unsigned int  k,w,checksum;
  unsigned int  secretkey[8],keydata[4],secretdata[24];
  //unsigned char *bufcur=buffer,*stext,*cursor;
  unsigned char *bufcur=buffer,*stext;
  unsigned char randstr[20];
  unsigned int secretblocks,words;
  size_t textlen=strlen((char *)text);
  size_t sbytes;

  if(textlen>56) nbExit("comAuthEncode: text too long for buffer - len=%d",textlen); // 2012-10-13 eat - checker
  skeSeedCipher(cipher);       /* generate random 128-bit CBC key */
  skeKeyData(keylen,keydata);  /* generate random Rijndael key data */  
  for(k=0;k<4;k++) secretkey[k]=htonl(cipher[k]);         /* pass in network byte order */
  for(k=0;k<keylen;k++) secretkey[k+4]=htonl(keydata[k]); /* pass in network byte order */
  bufcur+=pkeEncrypt(bufcur,identity->exponent,identity->modulus,(unsigned char *)&secretkey[0],(k+4)*4);
  nbClockToBuffer((char *)timeStamp);
  sprintf((char *)randstr,"%d",(int)(nbRandom()%INT_MAX));        /* generate time stamp */
  *(timeStamp+4)=*(randstr+0);
  *(timeStamp+7)=*(randstr+1);
  *(timeStamp+10)=*(randstr+2);
  *(timeStamp+13)=*(randstr+3);
  *(timeStamp+16)=*(randstr+4);
  *(timeStamp+19)=*(randstr+0);
  *(timeStamp+20)=0;
  memcpy(secretdata,timeStamp,20);        // put time stamp in secret data
  memcpy(secretdata+5,text,textlen+1);    // secretdata+5 is +20 bytes
  words=6+textlen/4;                      // words of secret data 
  stext=(unsigned char *)secretdata;      // pad last data word 
  sbytes=strlen((char *)stext)+1;
  secretblocks=words/4+1;                 /* number of 16 byte blocks */
  if(!RAND_bytes(stext+sbytes,secretblocks*16-sbytes))
    nbExit("comAuthEncode: OpenSSL random number generator not properly seeded");
  //for(cursor=stext+strlen((char *)stext)+1;cursor<stext+words*4;cursor++)
  //  *cursor=nbRandom();
  //secretblocks=words/4+1;                 /* number of 16 byte blocks */
  //for(k=words;k<(secretblocks*4-1);k++){  /* pad with random words */
  //  secretdata[k]=nbRand32();
  //  }
  skeKey(key,-keylen,keydata);                   /* generate decryption key */
  checksum=0;
  for(w=0;w<secretblocks*4-1;w++){
    secretdata[w]=ntohl(secretdata[w]);
    checksum+=secretdata[w];
    }
  secretdata[w]=checksum;  
  skeCipher(secretdata,secretblocks,cipher,key); /* use decryption key to encrypt */
  for(w=0;w<secretblocks*4;w++) secretdata[w]=htonl(secretdata[w]);
  memcpy(bufcur,secretdata,secretblocks*16);
  bufcur+=secretblocks*16;    
  return(bufcur-buffer);
  }

/*
*  Decode authentication block
*
*    Returns length of text string.  0 indicates failure.    
*/
unsigned int comAuthDecode(buffer,cipher,key,timeStamp,text,identity,blocklen)
  unsigned char *buffer;       /* Authentication packet buffer to decode */
  unsigned int  cipher[4];     /* Secret CBC key - extracted */
  skeKEY *key;                 /* Secret Rijndael key - extracted */
  unsigned char timeStamp[21]; /* Time stamp - extracted */ 
  unsigned char *text;         /* (request) client identity name, (reply) client time stamp */ 
  NB_PeerKey *identity;   /* For public key encryption of generated secret key */
  unsigned int blocklen; {     /* Length of authentication block */

  unsigned char *bufcur=buffer,*bufend,keylen,*cursor;
  unsigned int  secretkey[32],keydata[4],secretdata[24],len,k;
  unsigned int  checksum;
  unsigned int  secretblocks;

  if((unsigned)(*bufcur+16)>blocklen) return(0);      /* doesn't leave room for any secret text */  
  blocklen-=*bufcur;                        /* length of secret_text */
  len=pkeDecrypt(bufcur,identity->private,identity->modulus,(unsigned char *)&secretkey[0],96);
  bufcur+=*bufcur;
  if(len<20 || (len & 0x03)!=0) return(0);  /* key length < 20 or doesn't divid by 4 */
  if((blocklen & 0x0f)!=0) return(0);       /* secret text must divid by 16 */
  secretblocks=blocklen/16;
  if(secretblocks<1 || secretblocks>6) return(0); // 2012-10-17 eat - block count out of range
  for(k=0;k<4;k++) cipher[k]=ntohl(secretkey[k]); /* extract CBC cipher key */
  keylen=len/4-4;                           /* number of keydata words */  
  for(k=0;k<keylen;k++) keydata[k]=ntohl(secretkey[k+4]); /* extract Rijndael key data */
  skeKey(key,keylen,keydata);                    /* generate encryption key */
  memcpy(secretdata,bufcur,secretblocks*16);
  for(k=0;k<secretblocks*4;k++) secretdata[k]=ntohl(secretdata[k]);
  skeCipher(secretdata,secretblocks,cipher,key); /* use encryption key to decrypt */
  checksum=0;
  for(k=0;k<secretblocks*4-1;k++){
    checksum+=secretdata[k];
    secretdata[k]=htonl(secretdata[k]);
    }
  if(checksum!=secretdata[k]) return(0);  
  memcpy(timeStamp,secretdata,20); // 2012-02-09 eat - this is ok - ignore Checker
  *(timeStamp+20)=0;
  bufcur=(unsigned char *)secretdata;
  bufend=bufcur+secretblocks*16-4;
  cursor=text;
  for(bufcur+=20;bufcur<bufend && *bufcur!=0;bufcur++){
    *cursor=*bufcur;
    cursor++;
    }
  *cursor=0;
  return(strlen((char *)text)); 
  }

/**********************************************************************************
* Session Functions
**********************************************************************************/

/*
*  Create new session handle
*/
struct NBP_SESSION *nbpNewSessionHandle(NB_PeerKey *identity){
  struct NBP_SESSION *session;

  session=(struct NBP_SESSION *)nbAlloc(sizeof(struct NBP_SESSION));
  if(session==NULL) return(session);
  session->version=0;
  session->status=0;
  session->trancode=0;
  session->msgcode=0;
  session->option=0;
  session->selfIdentity=identity;
  session->peerIdentity=NULL;
  session->peer=NULL;
  session->channel=challoc();
  *(session->client_nonce)=0;
  *(session->server_nonce)=0;
  *(session->context)=0;
  *(session->buffer)=0;
  return(session);
  }

/*
*  Free session handle
*/
int nbpFreeSessionHandle(struct NBP_SESSION *session){
  if(session==NULL) return(-1);
  chfree(session->channel);
  nbFree(session,sizeof(struct NBP_SESSION));
  return(0);
  }

/*
*  Write a single NBP message
*
*    This should be revised to use a session handle instead of a channel handle
*    If len=0 then we assume a null terminated string
*/
int nbpMsg(struct NBP_SESSION *session,char trancode,char msgcode,char *text,size_t len){
  struct CHANNEL *channel=session->channel;
  struct NBP_MESSAGE *msgbuf;
  char buffer[NB_BUFSIZE];
  size_t prefix;

  msgbuf=(struct NBP_MESSAGE *)buffer;
  msgbuf->trancode=trancode;
  msgbuf->msgcode=msgcode;
  prefix=msgbuf->text-buffer;
  if(len==0) len=strlen(text)+1;
  if(len+prefix>NB_BUFSIZE){
    nbLogMsgI(0,'E',"nbpMsg: text too large for buffer - len=%d",len);
    return(-1);
    }
  memcpy(msgbuf->text,text,len);
  len+=msgbuf->text-buffer;
  len=chput(channel,buffer,len);
  if(len<0){
    nbLogMsgI(0,'E',"nbpMsg: chput call failed.");
    return(-1);
    }
  return(1);
  }

/*
*  Open a session with a peer (Connect)
*
*  Returns: 0 - success, -1 failure
*
*  A special context "@" is used to request that the peer
*  spawn a skull to handle the session.
*/
struct NBP_SESSION *nbpOpen(char nbp,NB_Term *peer,char *context){
  struct NBP_SESSION *session;
  char *brainName;
  struct BRAIN *brain;
  struct CHANNEL *channel;
  struct NBP_MESSAGE *msgbuf;

  char *ipaddr;
  unsigned short callport;
  int  len;
  unsigned char clientTime[21],serverTime[21],password[21];
  char *bufcur,*buffer;
  NB_PeerKey *identity,*myIdentity;
  unsigned int enCipher[4],deCipher[4]; /* CBC cipher key [0-3] */

  brain=(struct BRAIN *)peer->def;
  brainName=peer->word->value;
  /* Check for a session handle for this brain */
  if((session=brain->session)==NULL){
    if((myIdentity=brain->myIdentity)==NULL){
      myIdentity=nb_DefaultPeerKey;
      if(brain->myId!=NULL){
        myIdentity=nbpGetPeerKey(brain->myId);
        if(myIdentity==NULL){
          nbLogMsgI(0,'E',"Peer \"%s\" has unknown portrayed identity \"%s\".",brainName,brain->myId);
          return(NULL);
          }
        }
      brain->myIdentity=myIdentity;
      }
    if((session=nbpNewSessionHandle(myIdentity))==NULL){
      nbLogMsgI(0,'L',"nbpOpen() Unable to obtain a session handle.");
      return(NULL);
      }
    brain->session=session;
    session->peer=peer;
    }
  channel=session->channel;

  if(strlen(context)>255){
    nbLogMsgI(0,'L',"Context names greater than 255 are not supported by NBP at this time.");
    return(NULL);
    }
  strcpy(session->context,context);
  buffer=session->buffer;
  msgbuf=(struct NBP_MESSAGE *)buffer;
  if((identity=brain->identity)==NULL){
    if((identity=nbpGetPeerKey(brain->id))==NULL){
      nbLogMsgI(0,'E',"Peer \"%s\" has unknown identity \"%s\".",brainName,brain->id);
      return(NULL);
      }
    else brain->identity=identity;
    }

  ipaddr=brain->ipaddr;
  callport=brain->port;
  if(callport==0) ipaddr=brain->hostname;  // 2006-05-25 eat - local (unix) domain

  /* If a session is already open, return it. */
  /*   Note: the {} indicates we are reusing an open session */
  if(session->status!=0){
    nbLogMsgI(0,'I',"Peer %s=%s@%s:%u{%d}",brainName,brain->id,ipaddr,callport,brain->dsec);
    nbLogFlush(NULL);
    return(session);
    }
  /* otherwise, open a new session. */
  if(callport==0) nbLogMsgI(0,'I',"Peer %s=%s@%s",brainName,brain->id,ipaddr);
  else nbLogMsgI(0,'I',"Peer %s=%s@%s:%u",brainName,brain->id,ipaddr,callport);
  nbLogFlush(NULL);
 
  /* build authentication request message, cipher, key, and clientTime */
  bufcur=buffer;
  /* In the future we may want to use brain->spec as the starting protocol */
  /* if we already know the brain is capable of something higher than our bootstrap version */
  session->version=nbpboot;             /* start with bootstrap version */
  *bufcur=session->version; bufcur++;   /* set bootstrap version */
  if(session->version==1){              /* NBP1 */
    if(trace) nbLogMsgI(0,'T',"NBP1 SESSION[BEGIN] used by client");
    *bufcur=NBP_TRAN_SESSION; bufcur++;
    *bufcur=NBP_MSG_BEGIN;    bufcur++;
    }
  //else{                                 /* NBP0 */
  //  if(trace) nbLogMsgI(0,'T',"NBP0 authentication request used by client");
  //  *bufcur=0; bufcur++;         /* set authentication type */
  //  *bufcur=2; bufcur++;         /* set authentication format 2 - new to version 0.2.4 */
  //  }
  *bufcur=nbp; bufcur++;         /* set client capable specification */
  bufcur+=comAuthEncode((unsigned char *)bufcur,deCipher,&deKey,clientTime,(unsigned char *)((struct STRING *)session->selfIdentity->node.key)->value,identity,1);
  len=(bufcur-buffer);

  if(chopen(channel,ipaddr,callport)!=0){
    nbLogMsgI(0,'E',"Unable to connect to server.");
    return(NULL);
    }

  if(strcmp(context,"@")==0){     /* check for skull request */
    chstop(channel);              /* request a skull */
    *(session->context)=0;        /* clear context */
    }

  len=chput(channel,buffer,len);  
  if(len<0){
    nbLogMsgI(0,'E',"Error in authentication call to chput");
    nbpClose(session);
    return(NULL);
    }

  /* rework this section exiting on errors and handling NBP1 */
  *serverTime=0;  /* use serverTime as a success flag */
  if((len=chget(channel,buffer,NB_BUFSIZE))>0){
    bufcur=buffer;
    if(*bufcur==1 || *bufcur==NBP_TRAN_SESSION){ /* authentication challenge [0]*/
      bufcur++;
      if(*bufcur==2 || *bufcur==NBP_MSG_CHALLENGE){ /* format 2 [0]*/
        bufcur++;
        if(*bufcur>=nbpmin && *bufcur<=nbp){ /* in the expected range */
          session->version=*bufcur;  /* use negotiated version */
          brain->spec=*bufcur;       /* remember negotiated spec for future use */
          }
        else{
          nbLogMsgI(0,'L',"Unexpected NBP specification version returned by server.");
          nbpClose(session);
          return(NULL);
          }
        bufcur++;
        len=comAuthDecode(bufcur,enCipher,&enKey,serverTime,password,session->selfIdentity,len-3);
        if(len==0){
          nbLogMsgI(0,'E',"comAuthDecode error");
          nbpClose(session);
          return(NULL);
          }
        if(strcmp((char *)password,(char *)clientTime)!=0){ /* server authentication failure */
          nbLogMsgI(0,'E',"Peer %s identity %s authentication failed.",brainName,nbIdentityGetActive(NULL)->name->value);
          nbpClose(session);
          return(NULL);
          }
        /* worry about clock syncronization here if you want */
        nbLogFlush(NULL);
        chkey(channel,&enKey,&deKey,enCipher,deCipher);  /* start cipher text */
        strcpy(session->server_nonce,(char *)serverTime); /* make available to message routines */
        session->status=NBP_SERVER_AUTH;          /* we have authenticated the server */
     
        if(trace) nbLogMsgI(0,'T',"NBP%u negotiated",session->version);

        if(session->version==1){  /* send nonce and get confirmation */
          msgbuf->trancode=NBP_TRAN_SESSION;
          msgbuf->msgcode=NBP_MSG_RESPONSE;       
          strcpy(msgbuf->text,session->server_nonce);
          len=strlen(msgbuf->text)+1+msgbuf->text-buffer;
          if((len=chput(channel,buffer,len))<0){
            nbLogMsgI(0,'E',"nbpOpen: chput failed at call 2.");
            nbpClose(session);
            return(NULL);
            }
          len=chget(channel,buffer,NB_BUFSIZE);
          if(len<=0){
            nbLogMsgI(0,'E',"nbpOpen: chget failed at call 2.");
            nbpClose(session);
            return(NULL);
            }
          if(msgbuf->msgcode==NBP_MSG_HALT){
             nbLogMsgI(0,'E',"nbpOpen: %s",msgbuf->text);
             nbpClose(session);
             return(NULL);
            }
          if(msgbuf->trancode!=NBP_TRAN_SESSION || msgbuf->msgcode!=NBP_MSG_OK){
            nbLogMsgI(0,'L',"nbpOpen: Unrecognized authentication response from server");
            nbpClose(session);
            return(NULL);
            }
          session->status=NBP_AUTHENTICATED;
          }
        return(session);
        }
      else nbLogMsgI(0,'E',"Authentication challenge format %u not supported.",*bufcur);
      }
    else if(*bufcur==2){ /* authentication error - version 0 only */
      nbLogStr(NULL,buffer+2);  /* print error message  */
      }
    else{
      nbLogMsgI(0,'E',"Authentication reply type %u not recognized.",*buffer);
      }
    }
  chstop(channel);
  nbpClose(session);
  return(NULL);
  }

/*
*  Close a session with a peer (Disconnect)
*/
int nbpClose(struct NBP_SESSION *session){
  if(session==NULL) return(-1);
  if(session->channel!=NULL) chclose(session->channel);
  session->status=0;
  session->trancode=0;
  session->msgcode=0;
  session->option=0;
  nbLogFlush(NULL);
  return(0);
  }

/*
*  Send one command in a stream to peer interpreter
*
*  This function may be called multiple times in a session.  A
*  request header is included as required depending on session
*  status.  A channel stop is used to flag the end of a series
*  of commands.  This is inserted automatically on a disconnect
*  or a new type of request.     
*
*  Note: A context header is included on every line.  In the
*  future, the context should be exchanged during connection.
*
*/ 
int nbpPut(struct NBP_SESSION *session,char *command){ 
  char *bufcur,*buffer;
  struct NBP_MESSAGE *msgbuf;
  struct CHANNEL *channel=session->channel;
  int len;

  buffer=session->buffer;
  msgbuf=(struct NBP_MESSAGE *)buffer;
  bufcur=buffer;
 
  //if(session->version==0){
  //  /* Build authentication request message on first call - version 0 status */
  //  if(session->status==NBP_SERVER_AUTH){
  //    msgbuf->trancode=NBP_TRAN_AR;
  //    msgbuf->msgcode=1;   /* this is format=1 under version 0 */
  //    strcpy(msgbuf->text,session->server_nonce);
  //    bufcur=msgbuf->text+strlen(msgbuf->text)+1;   /* step over server nonce */
  //    }
  //  else if(session->status!=NBP_AUTHENTICATED){
  //    nbLogMsgI(0,'L',"nbpPut: Session status not recognized.");
  //    return(-1);
  //    }
  //  session->status=NBP_AUTHENTICATED;  
  //  }
  //else if(session->version==1){
  if(session->version==1){
    msgbuf->trancode=NBP_TRAN_EXECUTE;
    msgbuf->msgcode=NBP_MSG_BEGIN;
    bufcur=msgbuf->text;
    }
  strcpy(bufcur,session->context);
  strcat(bufcur," ");
  strcat(bufcur,command);
  len=bufcur+strlen(bufcur)+1-buffer;
  if((len=chput(channel,buffer,len))<0){
    nbLogMsgI(0,'E',"Error in command call to chput");
    nbpClose(session);
    return(-1);
    }
  while((len=chget(channel,buffer,NB_BUFSIZE))>0) nbLogStr(NULL,buffer);
  if(len<0){
    nbLogMsgI(0,'E',"nbpPut: chget failed at call 2.");
    nbLogFlush(NULL);
    nbpClose(session);
    return(-1);
    }
  if(session->version==1){
    len=chget(channel,buffer,NB_BUFSIZE);
    if(len<0){
      nbLogMsgI(0,'E',"nbpPut: chget failed at call 3.");
      nbpClose(session);
      return(-1);
      }
    if(msgbuf->trancode!=NBP_TRAN_EXECUTE || msgbuf->msgcode!=NBP_MSG_END){
      nbLogMsgI(0,'L',"nbpPut: Unexpected reply from server after STOP.");
      nbpClose(session);
      return(-1);
      }
    if(*(msgbuf->text)!=0) nbLogPutI("%s\n",msgbuf->text);  /* display server message */
    }
  return(0);
  }

void nbpStop(struct NBP_SESSION *session){
  chstop(session->channel);
  }         

/*
*  Send single command (complete session) 
*/
int nbpSend(int nbp,struct NB_TERM *brainTerm,char *context,char *command){
  if((currentSession=nbpOpen(nbp,brainTerm,context))==NULL){
    //nbLogMsgI(0,'T',"nbpSend: open failed");
    return(-1);
    }
  currentSession->option=NBP_OPT_CLOSE;
  /* Note: nbpPut() closes the session on error. */
  /* If there is an error, it is probably because an existing */
  /* but dropped session has been reused, so we try to open again. */
  if(nbpPut(currentSession,command)<0){
    if((currentSession=nbpOpen(nbp,brainTerm,context))==NULL){
      nbLogMsgI(0,'E',"Unable to reestablish a dropped session");
      return(-1);
      }
    if(nbpPut(currentSession,command)<0){
      nbLogMsgI(0,'E',"Unable to send command in reestablished session");
      return(-1);
      }
    }
  if(((struct BRAIN *)brainTerm->def)->dsec==0){
    nbpStop(currentSession);
    nbpClose(currentSession);
    }
  /* Leave the session open if a persistent connection */
  /* is requested in the brain definition.  This should*/
  /* not be used with servers prior to version 0.5.5   */
  /* because they will hang until the end of session.  */
  return(0);
  } 

/*
*  Begin a transaction with a peer
*    Must be called by a client on an open session in READY state
*/
int nbpBegin(struct NBP_SESSION *session,char trancode,char *text){
  int len;
  struct NBP_MESSAGE *msgbuf=(struct NBP_MESSAGE *)session->buffer;

  nbpMsg(session,trancode,NBP_MSG_BEGIN,text,0);
  len=chget(session->channel,session->buffer,NB_BUFSIZE);
  if(len<0){
    nbLogMsgI(0,'E',"nbpBegin: chget error reading transaction reply");
    return(-1);
    }
  if(msgbuf->trancode!=trancode){
    nbLogMsgI(0,'E',"nbpBegin: unexpected transaction code %u returned by server",msgbuf->trancode);
    return(-1);
    }
  if(msgbuf->msgcode==NBP_MSG_HALT){
    nbLogMsgI(0,'E',"nbpBegin: server halt - %s",msgbuf->text);
    return(-1);
    }
  if(msgbuf->msgcode!=NBP_MSG_OK){
    nbLogMsgI(0,'E',"nbpBegin: unexpected message code %u returned by server",msgbuf->msgcode);
    return(-1);
    }
  return(0);
  }

/*
*  nbpOpenTran - Open a session and begin a transaction
*/
struct NBP_SESSION *nbpOpenTran(char nbp,NB_Term *brainTerm,char trancode,char *text){
  struct NBP_SESSION *session;

  if((session=nbpOpen(nbp,brainTerm,"@"))==NULL){   /* request skull brain */
    nbLogMsgI(0,'E',"nbpOpenTran: Unable to connect to peer brain");
    return(NULL);
    }

  if(nbpBegin(session,trancode,text)<0){
    nbLogMsgI(0,'E',"nbpOpenTran: nbpBegin failed");
    nbpClose(session);
    return(NULL);
    }

  return(session);
  }

/* End a transaction without closing the session 
*/
int nbpEnd(struct NBP_SESSION *session,char trancode){
  int len;
  struct NBP_MESSAGE *msgbuf=(struct NBP_MESSAGE *)session->buffer;

  if(trace) nbLogMsgI(0,'T',"nbpEnd called");
  nbpStop(session);
  if(trace) nbLogMsgI(0,'T',"nbpStop issued");
  len=chget(session->channel,session->buffer,NB_BUFSIZE);
  if(trace) nbLogMsgI(0,'T',"back from chget");
  if(len<0){
    nbLogMsgI(0,'E',"nbpEnd() chget error %d after stop");
    return(-1);
    }
  if(msgbuf->trancode!=trancode){
    nbLogMsgI(0,'E',"nbpEnd() unexpected transaction code %u returned by server after stop",msgbuf->trancode);
    return(-1);
    }
  if(msgbuf->msgcode==NBP_MSG_HALT){
    nbLogMsgI(0,'E',"nbpEnd() server halt after stop - %s",msgbuf->text);
    return(-1);
    }
  if(msgbuf->msgcode!=NBP_MSG_END){
    nbLogMsgI(0,'E',"nbpEnd() unexpected message code %u returned by server after stop",msgbuf->msgcode);
    return(-1);
    }
  return(0);
  }

int nbpCloseTran(struct NBP_SESSION *session,char trancode){
  int len;
  struct NBP_MESSAGE *msgbuf=(struct NBP_MESSAGE *)session->buffer;

  if(trace) nbLogMsgI(0,'T',"nbpCloseTran called");
  nbpStop(session); 
  if(trace) nbLogMsgI(0,'T',"nbpStop issued");
  len=chget(session->channel,session->buffer,NB_BUFSIZE);
  if(trace) nbLogMsgI(0,'T',"back from chget");
  nbpStop(session);  /* this will work, but we should send SESSION[END] */
  nbpClose(session);
  if(len<0){
    nbLogMsgI(0,'E',"nbpCloseTran: chget error %d after stop");
    return(-1);
    }
  if(msgbuf->trancode!=trancode){
    nbLogMsgI(0,'E',"nbpCloseTran: unexpected transaction code %u returned by server after stop",msgbuf->trancode);
    return(-1);
    }
  if(msgbuf->msgcode==NBP_MSG_HALT){
    nbLogMsgI(0,'E',"nbpCloseTran: server halt after stop - %s",msgbuf->text);
    return(-1);
    }
  if(msgbuf->msgcode!=NBP_MSG_END){
    nbLogMsgI(0,'E',"nbpCloseTran: unexpected message code %u returned by server after stop",msgbuf->msgcode);
    return(-1);
    }
  nbLogMsgI(0,'I',"Session closed");
  return(0);
  }


/*
*  Prototype File Transmit
*
*    copy <mode> [@<src_brain>:]<src_file> [@<dst_brain>:]<dst_file>
*
*    mode='b' binary (all other options are ascii)
*    mode='a' ascii file 
*    mode='q' queued commands
*    mode='c' queued command file
*    mode='t' queued text file
*    mode='p' queued package
*
*  If the src or dst brain term arguments are NULL, we interpret the
*  file names as local files.  The 'Q' mode, expects a brain name in
*  the destination file argument.  It is used by the peer to locate
*  the destination queue path.
*
*  Returns:
*    -1 - error
*     0 - File not copied
*     1 - File copied
*/
int nbpCopy(int nbp,NB_Term *srcBrainTerm,char *srcFile,NB_Term *dstBrainTerm,char *dstFile,char mode){
  struct NBP_SESSION *srcSession=NULL,*dstSession=NULL;
  FILE *srcfile=NULL,*dstfile=NULL;
  int len;
  char buffer[NB_BUFSIZE];
  char text[256];
  char *openmode;

  *text=mode;
  *(text+1)=':';
  if(srcBrainTerm!=NULL){
    if(trace) nbLogMsgI(0,'T',"opening session with source brain");
    if(strlen(srcFile)+3>sizeof(text)){
      nbLogMsgI(0,'E',"nbpCopy: source file name too long for buffer '%s'",srcFile);
      return(-1);
      }
    strcpy(text+2,srcFile);
    srcSession=nbpOpenTran(nbp,srcBrainTerm,NBP_TRAN_GETFILE,text);
    if(srcSession==NULL) return(-1);
    }
  else{
    if(mode=='b') openmode="rb";
    else openmode="r";
    if((srcfile=fopen(srcFile,openmode))==NULL){
      nbLogMsgI(0,'E',"nbpCopy: Unable to open source file '%s'",srcFile);
      return(-1);
      }
    }
  if(dstBrainTerm!=NULL){
    if(trace) nbLogMsgI(0,'T',"opening session with destination brain");
    if(strlen(dstFile)+3>sizeof(text)){
      nbLogMsgI(0,'E',"nbpCopy: destination file name too long for buffer '%s'",srcFile);
      return(-1);
      }
    strcpy(text+2,dstFile);
    dstSession=nbpOpenTran(nbp,dstBrainTerm,NBP_TRAN_PUTFILE,text);
    if(dstSession==NULL){
      if(srcfile!=NULL) fclose(srcfile);
      else nbpClose(srcSession);
      return(-1);
      }
    }
  else{
    if(mode=='b') openmode="wb";
    else openmode="w";
    if((dstfile=fopen(dstFile,openmode))==NULL){
      nbLogMsgI(0,'E',"nbpCopy: Unable to open destination file '%s'",dstFile);
      return(-1);
      }
    }

  /* NOTE: We might consider adding file io as an option for the
           channel functions.  This would simplify the code here because
           the permutations would be handled by chget and chput.
           It would also enable file encryption.
  */

  len=1;
  while(len>0){
    if(srcfile!=NULL){
      if(mode=='b') len=fread(buffer,1,4000,srcfile);
      else{
        if(fgets(buffer,NB_BUFSIZE,srcfile)==NULL) len=0;
        else len=strlen(buffer);
        }
      }
    else len=chget(srcSession->channel,buffer,NB_BUFSIZE);
    if(len<0) nbLogMsgI(0,'E',"nbpCopy: chget error %d",len);
    if(len>0){
      if(dstfile!=NULL){
        if(mode=='b') len=fwrite(buffer,1,len,dstfile);
        else{
	  len=fputs(buffer,dstfile);
	  if(len==0) len=1;
          }
 	if(len<0) nbLogMsgI(0,'E',"nbpCopy write error %d",len);
        }
      else{
	len=chput(dstSession->channel,buffer,len);
        if(len<=0) nbLogMsgI(0,'E',"nbpCopy: chput error %d",len);
	}
      }
    }
 
  if(srcfile!=NULL) fclose(srcfile);
  if(dstfile!=NULL) fclose(dstfile);

  if(srcSession!=NULL) nbpCloseTran(srcSession,NBP_TRAN_GETFILE);
  if(dstSession!=NULL) nbpCloseTran(dstSession,NBP_TRAN_PUTFILE);

  nbLogMsgI(0,'I',"Copy completed");
  return(1);
  }

/**********************************************************************************
* Server Side Functions and Transactions
**********************************************************************************/

/*
*  Server's authentication transaction
*
*  SESSION[BEGIN]
*
*  NOTE: This logic was written to support both NBP0 and NBP1 authentication
*  because there is very little difference.  The primary changes are in the
*  first two bytes of each message, and NBP1 includes a confirming
*  SESSION[OK] message to complete the authentication sequence.  NBP0 clients
*  include the first command of a possible series as part of the last
*  authentication message.  This function ignores it, so the calling routine
*  must look for it in session->buffer when this function returns successfully.
*
*  Return Codes:
*
*    0 - Successful authentication of server and client
*   -1 - Failed to authenticate.
*/
int nbpServeAuth(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf,int len){
  NB_PeerKey *clientKey; 
  char clientTime[21],serverTime[21];
  char clientId[64];                /* client identity name */
  unsigned int enCipher[4],deCipher[4]; /* CBC cipher key [0-3] */
  nbIDENTITY clientIdentity;

  struct CHANNEL *channel=session->channel;
  char trancode,msgcode,nbpversion,*buffer=session->buffer,*bufcur;

  /* verify trancode and msgcode here */
  if(session->version==1){
    if(msgbuf->trancode!=NBP_TRAN_SESSION){
      nbpMsg(session,NBP_TRAN_SESSION,NBP_MSG_HALT,"NBP001E Unexpected transaction code",0);
      return(-1);
      }
    if(msgbuf->msgcode!=NBP_MSG_BEGIN){
      nbpMsg(session,NBP_TRAN_SESSION,NBP_MSG_HALT,"NBP001E Unexpected message code",0);
      return(-1);
      }
    }
  else{  /* version 0 */
    if(msgbuf->trancode!=0){
      nbLogMsgI(0,'E',"NBP0 Bootstrap transaction %u not supported",msgbuf->trancode);
      return(-1);
      }
    if(msgbuf->msgcode!=2){
      nbLogMsgI(0,'E',"NBP0 Authentication request format %u not supported",msgbuf->msgcode);
      return(-1);
      }
    }

  bufcur=msgbuf->text;
  nbpversion=*bufcur;                   /* client capable NBP version */
  if(nbpversion>nbp) nbpversion=nbp;    /* downgrade if necessary */
  bufcur++;  /* step over client capable NBP version */
  if(comAuthDecode(bufcur,enCipher,&clientEnKey,clientTime,clientId,session->selfIdentity,len-3)==0){
    *clientId=0;  /* this should be in comAuthDecode */
    }
  if(session->version==0){
    trancode=NBP_TRAN_AF;  /* authentication failure */
    msgcode=0;            /* format 0 */
    }
  else{
    trancode=NBP_TRAN_SESSION;
    msgcode=NBP_MSG_HALT;
    }
  if(nbpversion<nbpmin){
    sprintf(msgbuf->text,"NBP000E Server does not support NBP%u client",nbpversion);
    nbpMsg(session,trancode,msgcode,msgbuf->text,0);
    nbLogStr(NULL,msgbuf->text);
    return(-1);
    }
  if(*clientId==0){
    sprintf(msgbuf->text,"NBP000E Authentication request not recognized.\n");
    nbpMsg(session,trancode,msgcode,msgbuf->text,0);
    nbLogStr(NULL,msgbuf->text);
    return(-1);
    }
  if((clientIdentity=nbIdentityGet(NULL,clientId))==NULL){
    snprintf(msgbuf->text,(size_t)NB_BUFSIZE,"NB000E Client identity \"%s\" not recognized.\n",clientId); //2012-01-31 dtl replaced sprintf
    nbpMsg(session,trancode,msgcode,msgbuf->text,0);
    nbLogStr(NULL,msgbuf->text);
    return(-1);
    }
  nbIdentitySetActive(NULL,clientIdentity);
  if((clientKey=nbpGetPeerKey(clientId))==NULL){
    sprintf(msgbuf->text,"NB000E Client identity \"%s\" not recognized.\n",clientId);
    nbpMsg(session,trancode,msgcode,msgbuf->text,0);
    nbLogStr(NULL,msgbuf->text);
    return(-1);
    }
  if(clientKey->identity==NULL) clientKey->identity=clientIdentity;
  if(!(clientIdentity->authority&AUTH_CONNECT)){
    sprintf(msgbuf->text,"NB000E Client identity \"%s\" not authorizated to connect.\n",clientId);
    nbpMsg(session,trancode,msgcode,msgbuf->text,0);
    nbLogStr(NULL,msgbuf->text);
    return(-1);
    }
  /* build authentication reply/challenge */
  msgbuf=(struct NBP_MESSAGE *)buffer;
  if(session->version==0){
    if(trace) nbLogMsgI(0,'T',"NBP0 authentication challenge will be used");
    msgbuf->trancode=NBP_TRAN_AC;  /* authentication challenge (server response) */
    msgbuf->msgcode=2;            /* format 2 */
    }
  else{
    if(trace) nbLogMsgI(0,'T',"NBP1 authentication challenge will be used");
    msgbuf->trancode=NBP_TRAN_SESSION;
    msgbuf->msgcode=NBP_MSG_CHALLENGE;
    }
  *(msgbuf->text)=nbpversion;       /* negotiated specification */
  if(trace) nbLogMsgI(0,'T',"NBP%u session selected",nbpversion);
  bufcur=msgbuf->text+1;
  bufcur+=comAuthEncode((unsigned char *)bufcur,deCipher,&clientDeKey,(unsigned char *)serverTime,(unsigned char *)clientTime,clientKey,1);
  len=bufcur-buffer;
  len=chput(channel,buffer,len);  /* send reply/challenge */
  if(len<0){
    nbLogMsgI(0,'E',"nbpServeAuth: chput failed.");
    return(-1);
    }
  chkey(channel,&clientEnKey,&clientDeKey,enCipher,deCipher);
  len=chget(channel,buffer,NB_BUFSIZE);
  if(len<0){
    nbLogMsgI(0,'E',"nbpServeAuth: chget failed.");
    return(-1);
    }
  if(len==0){
    nbLogMsgI(0,'E',"nbpServeAuth: client returned stop in response to authentication challenge");
    return(-1);
    }
  session->version=nbpversion;      /* use negotiated version */
  if(session->version==0){
    if(msgbuf->trancode!=NBP_TRAN_AR){
      nbLogMsgI(0,'E',"nbpServeAuth: Request %u not supported.",msgbuf->trancode);
      return(-1);
      }
    if(msgbuf->msgcode!=1){
      nbLogMsgI(0,'E',"nbpServeAuth: Command request format %u not supported.",msgbuf->msgcode);
      return(-1);
      }
    if(strcmp(msgbuf->text,serverTime)!=0){
      nbLogMsgI(0,'E',"nbpServeAuth: Identity \"%s\" failed to authenticate.",clientId);
      return(-1);
      }
    nbLogHandlerAdd(NULL,currentSession,nbpClientOut);
    //outStream(2,&nbpClientOut);
    //serving=1;
    nbLogMsgI(0,'I',"NBP0 Peer %s@%s",clientId,channel->ipaddr);
    //outStream(2,NULL);
	nbLogHandlerRemove(NULL,currentSession,nbpClientOut);
    session->peerIdentity=clientKey;
    return(0);
    }
  /* NBP1 from here on */
  if(msgbuf->trancode!=NBP_TRAN_SESSION){
    nbLogMsgI(0,'E',"nbpServeAuth: Unexpected NBP transaction code(%u) returned by client.",msgbuf->trancode);
    return(-1);
    }
  if(msgbuf->msgcode==NBP_MSG_HALT) return(-1); /* client is aborting authentication */
  if(msgbuf->msgcode!=NBP_MSG_RESPONSE){
    nbLogMsgI(0,'E',"nbpServeAuth: Unexpected NBP message code(%u) returned by client.",msgbuf->msgcode);
    return(-1);
    }
  if(strcmp(msgbuf->text,serverTime)!=0){
    nbLogMsgI(0,'E',"nbpServeAuth: Identity \"%s\" failed to authenticate.",clientId);
    return(-1);
    }
/*
  nbLogMsgI(0,'I',"NBP%u Peer %s@%s",session->version,clientId,channel->ipaddr);
*/
  nbpMsg(session,NBP_TRAN_SESSION,NBP_MSG_OK,"",0);
  session->peerIdentity=clientKey;
  return(0);
  }

/*
*  Server's EXECUTE transaction
*/
void nbpServeExecute(struct LISTENER *ear,struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf){
  struct CHANNEL *channel=session->channel;
  nbCELL context=(nbCELL)ear->context;

//  if(ear==NULL) context=locGloss;
//  else context=ear->context;
  //nbLogPutI(">%s\n",msgbuf->text);
  nbLogFlush(context);
  nbLogHandlerAdd(context,session,nbpClientOut);
  //outStream(2,&nbpClientOut);
  //serving=1;
  //nbCmdSid((nbCELL)context,msgbuf->text,0,clientIdentity);
  //2009-02-28 eat
  //nbCmdSid((nbCELL)context,msgbuf->text,1,clientIdentity);
  nbCmdSid((nbCELL)context,msgbuf->text,1,session->peerIdentity->identity);
  nbLogFlush(context);
  nbLogHandlerRemove(context,session,nbpClientOut);
  //outStream(2,NULL);
  //serving=0;
  chstop(channel); /* let peer know the command output is complete */
  /* send EXECUTE[END] - get return code from eStatement */
  nbpMsg(session,NBP_TRAN_EXECUTE,NBP_MSG_END,"",0);
  }

/*
*  Server's PUTFILE transaction
*
*  Text Format:
*
*     {a|b}:filename
*
*     a - ascii
*     b - binary
*
*     Queue Options
*
*     {q|c|t|p}:brainname
*
*     q - queue
*     c - command
*     t - text
*     p - package
*/
void nbpServePutFile(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf){
  FILE *file;
  int len;
  size_t size;
  struct CHANNEL *channel=session->channel;
  char *buffer=session->buffer;
  char mode,*openmode,filename[256];
  NB_Term *clientTerm=NULL,*brainTerm=NULL;
  NB_Node *node;
  nbClient *client;
  struct BRAIN *brain;

  mode=*(msgbuf->text);
  if(mode!='a' && mode!='b'){
    char dirname[512];
    //if((brainTerm=(NB_Term *)nbTermLocate((nbCELL)brainC,msgbuf->text+2))==NULL){
    if((clientTerm=(NB_Term *)nbTermLocate((nbCELL)session->peer,msgbuf->text+2))==NULL){
      //nbLogMsgI(0,'T',"target brain is '%s'",msgbuf->text+2);
      nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_HALT,"NBP000E PUTFILE: target node not defined",0);
      return;
      }
    node=(NB_Node *)clientTerm->def;
    client=(nbClient *)node->knowledge;
    brainTerm=client->brainTerm;
    brain=(struct BRAIN *)brainTerm->def;
    if((NB_Term *)brain->context!=clientTerm){
      nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_HALT,"NBP000E PUTFILE: target node not peer.client or logic error",0);
      return;
      }
    if(nbqGetDir(dirname,brainTerm)<0){
      nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_HALT,"NBP000E PUTFILE: target queue not defined",0);
      return;
      }
    nbQueueGetFile(filename,dirname,nbIdentityGetActive(NULL)->name->value,brain->qsec,NBQ_UNIQUE,mode);
    /* horse with the file name to use nbQueueCommit() instead of file locking */
    *(filename+strlen(filename)-2)='%'; 
    }
  else strcpy(filename,msgbuf->text+2);
  if(mode=='b') openmode="wb";
  else openmode="w";
  if((file=fopen(filename,openmode))==NULL){
    nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_HALT,"NBP000E PUTFILE: unable to open destination file",0);
    return;
    } 
  nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_OK,"",0);
  len=chget(channel,buffer,NB_BUFSIZE);
  if(mode=='b') while(len>0){
    size=len;
    fwrite(buffer,1,size,file);     /* should check for errors */
    len=chget(channel,buffer,NB_BUFSIZE);
    }
  else while(len>0){
    fputs(buffer,file);
    len=chget(channel,buffer,NB_BUFSIZE);
    }
  fclose(file);
  if(len<0){
    /* send PUTFILE[HALT] */
    nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_HALT,"NBP000E PUTFILE: chget error",0);
    return;
    }
  if(brainTerm!=NULL) nbQueueCommit(filename);
  /* send PUTFILE[END] */
  nbpMsg(session,NBP_TRAN_PUTFILE,NBP_MSG_END,"NBP000I PUTFILE: File saved",0);
  }

/*
*  Server's GETFILE transaction
*
*  Text Format:
*
*     {a|b}:filename
*
*     a - ascii
*     b - binary
*
*     Queue Options
*
*     {q|c|t|p}:brainname
*
*     q - queue
*     c - command
*     t - text
*     p - package
*/
void nbpServeGetFile(struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf){
  FILE *file;
  int len=1;
  struct CHANNEL *channel=session->channel;
  char *buffer=session->buffer;
  char mode,*openmode,filename[256];

  mode=*(msgbuf->text);
  strcpy(filename,msgbuf->text+2);

  if(mode=='b') openmode="rb";
  else openmode="r";
  if((file=fopen(filename,openmode))==NULL){
    nbpMsg(session,NBP_TRAN_GETFILE,NBP_MSG_HALT,"NBP000E GETFILE: unable to open source file",0);
    return;
    }
  nbpMsg(session,NBP_TRAN_GETFILE,NBP_MSG_OK,"",0);
  if(mode=='b') while(len>0){
    len=fread(buffer,1,4000,file);
    if(len>0) len=chput(channel,buffer,len);
    }
  else while(fgets(buffer,4000,file)!=NULL){
    len=chput(channel,buffer,strlen(buffer));
    }
  nbpStop(session);
  fclose(file);
  if(len<0){
    /* send GETFILE[HALT] */
    nbpMsg(session,NBP_TRAN_GETFILE,NBP_MSG_HALT,"NBP000E GETFILE: read or communication error",0);
    return;
    }
  /* send GETFILE[END] */
  nbpMsg(session,NBP_TRAN_GETFILE,NBP_MSG_END,"NBP000I GETFILE: File returned",0);
  }

/*
*  Server's SESSION transaction
*/
void nbpServeSession(struct LISTENER *ear,struct NBP_SESSION *session,struct NBP_MESSAGE *msgbuf,int len){
  struct CHANNEL *channel=session->channel;
  char *buffer=session->buffer;
#if defined(WIN32)
  LARGE_INTEGER alarmTime;        /* timer intervals - SetWaitableTimer */
  HANDLE nbpTimer=NULL;           /* NBP timeout timer handle */

  nbpTimer=CreateWaitableTimer(NULL,TRUE,NULL);
  if(!nbpTimer){
    nbLogMsgI(0,'L',"CreateWaitableTimer failed (%d)", GetLastError());
    exit(NB_EXITCODE_FAIL);
    }
#endif
  if(nbpServeAuth(session,msgbuf,len)){ /* do authenication transactions */
    nbpClose(session);
    nbpFreeSessionHandle(session);
    return;
    }
  if(ear!=NULL){
    if(trace) nbLogMsgI(0,'T',"nbpServeSession: calling cloneListener");
    //cloneListener(ear,listenerTypeNbpSession,session->channel->socket,session);
    nbpCloneListener(ear,session->channel->socket,session);
    return;
    }
  /* The following code is here to support skulls */
  msgbuf=(struct NBP_MESSAGE *)buffer;
  len=chget(channel,buffer,NB_BUFSIZE);                /* READY state get */
  while(len>0){
    nbClockAlert();            /* advance time */
    switch(msgbuf->trancode){
      case NBP_TRAN_EXECUTE:   nbpServeExecute(ear,session,msgbuf); break;
      case NBP_TRAN_PUTFILE:   nbpServePutFile(session,msgbuf); break;
      case NBP_TRAN_GETFILE:   nbpServeGetFile(session,msgbuf); break;
      }
    if(!skull_socket){
#if defined(WIN32)
      alarmTime.QuadPart=-50000000; /* 5 seconds */
      if(!SetWaitableTimer(nbpTimer,&alarmTime,0,(PTIMERAPCROUTINE)winAlarmTimeout,NULL,0))
        nbLogMsgI(0,'L',"SetWaitableTimer failed (%d)", GetLastError());
#else   
      signal(SIGALRM,alarmTimeout);
      alarm(5);  /* make sure the peer isn't hung */
#endif
      }
    if((len=chget(channel,buffer,NB_BUFSIZE))<0){   /* READY state get */
      nbLogMsgI(0,'L',"nbpServe: chget failed at call 5.");
      len=0;
      }
#if defined(WIN32)
    CancelWaitableTimer(nbpTimer);
#else    
    alarm(0);
#endif
    }
  nbpClose(session);
  return;
  }

/*
*  Serve client requests
*
*    Authenticate a client and handle transactions
*
*    If upgrade is zero, the NBP version will remain at the minimum level
*    specified by the client.  Otherwise the session will be upgraded to 
*    the highest level supported by both client and server.
*/
void nbpServe(struct LISTENER *ear,struct NBP_SESSION *session,int nbp,char *oar){
  int len;
//  char clientTime[21];
//  char serverTime[21];
  char *buffer=session->buffer,*bufcur;
//  char clientId[64];                /* client identity name */
//  unsigned int enCipher[4],deCipher[4]; /* CBC cipher key [0-3] */
  char stop=1;   /* flag to avoid extra stop */
  struct CHANNEL *channel=session->channel;

  currentSession=session;  // set global variable for clientPrint function
  if((len=chget(channel,buffer,NB_BUFSIZE))<0){
    nbLogMsgI(0,'E',"chget failed at call 3.");
    nbpClose(session);
    return;
    }
  if(len==0){   /* experimental request for dedicated brain */
    //sprintf(buffer,"-socket=%d,ipaddr=\"%s\",oar=\"%s\"",channel->socket,channel->ipaddr,serveoar);
    sprintf(buffer,":define skull node peer.skull(%d,\"%s\");",channel->socket,((struct STRING *)session->selfIdentity->node.key)->value);
    //nbLogMsgI(0,'I',"Spawning skull: %s",buffer);
    if(skull_socket){
      nbLogMsgI(0,'L',"A skull is not currently allowed to spawn a skull - terminating.");
      nbLogFlush(NULL);
      exit(NB_EXITCODE_FAIL);
      }
#if defined(WIN32)
    SetHandleInformation((HANDLE)channel->socket,HANDLE_FLAG_INHERIT,0);
#else
    fcntl(channel->socket,F_SETFD,0);  // don't close on exec
#endif
    nbSpawnSkull((nbCELL)ear->context,oar,buffer);
    nbpClose(session);
    nbpFreeSessionHandle(session);
    return;
    }

  bufcur=buffer;
  if(trace) nbLogMsgI(0,'T',"NBP%u bootstrap connection requested",*bufcur);
  if(*bufcur==1 && nbp){  /* NBP1 connection request? */
    session->version=1;
    nbpServeSession(ear,session,(struct NBP_MESSAGE *)(buffer+1),len-1);
    return;
    }
  // 2008-03-25 eat - version 0 support removed
  else nbLogMsgI(0,'E',"Specification %u not supported.",*bufcur);

  if(stop==1) chstop(channel);   /* 2002/02/17  - eat - version 0.3.0 */
  chclose(channel);
  }

/*=======================================================================
*  Routines from the original nbqueue.c that are nbp specific
*/

/*
* Open a queue file for output
*
* Option:
*   NBQ_INTERVAL - time interval shared queue file
*   NBQ_UNIQUE   - unique queue file
*
* Type:
*   q - command queue
*   c - command file
*   t - text file
*   p - package
*
*/
NBQFILE nbqOpen(NB_Term *brainTerm,int option,int type,char *filename){
  char dirname[512];
  NBQFILE file;
  struct BRAIN *brain=(struct BRAIN *)brainTerm->def;

  if(nbqGetDir(dirname,brainTerm)<0) return(NBQFILE_ERROR);
  nbQueueGetFile(filename,dirname,brain->myId,brain->qsec,option,type);
  file=nbQueueOpenFileName(filename,NBQ_WAIT,NBQ_PRODUCER);
  return(file);
  }

/*
*  Store Command in Brain's Queue
*/
int nbqStoreCmd(NB_Term *brainTerm,char *cursor){
  struct BRAIN *brain=(struct BRAIN *)brainTerm->def;
  char buf[NB_BUFSIZE];
  char filename[256];
  //char *dir=((struct BRAIN *)brainTerm->def)->dir->value;
  size_t wpos,size;
  NBQFILE file;

  if((file=nbqOpen(brainTerm,NBQ_INTERVAL,'q',filename))<0) return(-1);
  /*
  *  Append command to queue file
  */
  wpos=nbQueueSeekFile(file,-1);
  if(wpos<brain->qfsize || brain->qfsize==0){
#if defined(WIN32)
    sprintf(buf,">%s\r\n",cursor);  // CR LF on Windows at this level
#else
    sprintf(buf,">%s\n",cursor);
#endif
    size=(size_t)strlen(buf);
    if(wpos<0 || nbQueueWriteFile(file,buf,size)!=size){
      nbQueueCloseFile(file);
      nbLogMsgI(0,'E',"Append to %s failed.",filename);
      return(-1);
      }
#if defined(WIN32)
    SetEndOfFile(file); /* is this necessary? */
#endif
    nbQueueCloseFile(file);
    if(trace) nbLogMsgI(0,'I',"Command queued to %s at offset %d",filename,wpos);
    return(0);
    }
  nbQueueCloseFile(file);
  nbLogMsgI(0,'I',"Command refused - queue file size limit reached at %d",wpos);
  return(-1);
  }

/*
*  Get name of queue directory for a given brain.
*/
int nbqGetDir(char *qname,NB_Term *brainTerm){
  char *dir;
  char *brainName=brainTerm->word->value;
  struct BRAIN *brain=(struct BRAIN *)brainTerm->def;

  *qname=0;
  if(brain->dir==NULL){
    nbLogMsgI(0,'E',"Queue directory not specified.");
    return(-1);
    }
  //if(brain->dir==NULL) dir=quedir;
  dir=brain->dir->value;
  if(brain->version<1) sprintf(qname,"%s/%s",dir,brainName);
  else strcpy(qname,dir);
  return(0);
  }

/*
*  Send ".q" commands direct to a peer brain.
*
*    -1 Unable to communicate with peer
*/
int nbqSendDirect(qHandle,brainTerm)
  struct NBQ_HANDLE *qHandle;
  NB_Term *brainTerm; {
  struct NBP_SESSION *session;
  char *cmd;

  if(trace) nbLogMsgI(0,'T',"nbqSendDirect() called");
  if((session=nbpOpen(nbp,brainTerm,""))==NULL){
    nbQueueCloseFile(qHandle->file);
    nbLogMsgI(0,'E',"Unable to open a session with %s",brainTerm->word->value);
    return(-1);
    }
  while((cmd=nbQueueRead(qHandle))!=NULL){
    nbLogPutI("> %s\n",cmd+1);
    if(nbpPut(session,cmd+1)!=0){
      nbQueueCloseFile(qHandle->file);
      nbpClose(session);
      nbLogMsgI(0,'W',"Unable to send commands to peer at this time.");
      return(-1);
      }
    }
  /* Close the session if a persistent session was not requested */
  /* We should consider having a persistence option in the session structure */
  if(((struct BRAIN *)session->peer->def)->dsec==0){
    nbpStop(session);
    nbpClose(session);
    }
  nbLogMsgI(0,'I',"Queue file %s forwarded to %s",qHandle->filename,brainTerm->word->value);
  if(remove(qHandle->filename)<0) nbLogMsgI(0,'L',"Remove failed - %s",qHandle->filename);
  nbQueueCloseFile(qHandle->file);
  return(0);
  }

/*
*  Send any queue file type to a peer brain associated with an open session
*/
void nbqSendFile(qHandle,session,dstQueBrainName)
  struct NBQ_HANDLE *qHandle;
  struct NBP_SESSION *session;
  char *dstQueBrainName; {

  FILE *file;
  char text[256],*openmode;
  int len;
  char buffer[NB_BUFSIZE];

  *text=qHandle->entry->type;
  *(text+1)=':';
  len=snprintf(text+2,sizeof(text)-2,"%s",dstQueBrainName); //2012-01-31 dtl replace strcpy
  if(len>=sizeof(text)-2){
    nbLogMsgI(0,'E',"nbqSendFile: destination queue name too long for buffer - '%s'",dstQueBrainName);
    return;
    }
// closing the file here releases the lock
// this is a bad thing
// however, we must have done this because of another bad thing
// so let's see what that problem is again
//#if defined(WIN32)
//  nbQueueCloseFile(qHandle->file);
//#endif
  if(nbpBegin(session,NBP_TRAN_PUTFILE,text)<0){
    nbLogMsgI(0,'E',"nbpOpenTran: nbpBegin failed");
    nbpClose(session);
    nbpFreeSessionHandle(session);
    return;
    }
  if(*text=='b') openmode="rb";
  else openmode="r";
  if((file=fopen(qHandle->filename,openmode))==NULL){
    nbLogMsgI(0,'E',"nbqSendFile: Unable to open source file '%s'",qHandle->filename);
    return;
    }
  if(trace) nbLogMsgI(0,'T',"nbqSendFile() copying file %s",qHandle->filename);
  len=1;
  while(len>0){
    if(*text=='b') len=fread(buffer,1,4000,file);
    else{
      if(fgets(buffer,NB_BUFSIZE,file)==NULL) len=0;
      else len=strlen(buffer);
      }
    if(len>0){
      len=chput(session->channel,buffer,len);
      if(len<=0) nbLogMsgI(0,'E',"nbqSendFile() chput error %d",len);
      }
    }
  fclose(file);
  if(nbpEnd(session,NBP_TRAN_PUTFILE)==0){
    nbLogMsgI(0,'I',"Queue file %s forwarded to %s",qHandle->filename,session->peer->word->value);
    if(remove(qHandle->filename)<0) nbLogMsgI(0,'L',"Remove failed - %s",qHandle->filename);
    }
// close the file to release the lock
//#if !defined(WIN32)
  nbQueueCloseFile(qHandle->file);
//#endif
  }

/*
*  Send NodeBrain Queue to a peer brain
*    In the future we need to be able to send to a list of brains.
*/
void nbqSend(NB_Term *brainTerm){
  struct NBP_SESSION *session=NULL;
  struct NBQ_HANDLE *qHandle;
  struct NBQ_ENTRY  *qEntry;
  int direct=0,status=0;
  char *dstQueBrainName;
  char dirname[512];
  struct BRAIN *brain=(struct BRAIN *)brainTerm->def;

  if(trace) nbLogMsgI(0,'T',"nbqSend() called"); 
  /*
  *  Use the target queue name if defined, else use brain name
  *      declare <brain> brain id@host:port[queue,target]
  */
  if(brain->skullTarget==NULL){
    dstQueBrainName=brainTerm->word->value;
    direct=1;
    }
  else dstQueBrainName=brain->skullTarget->value;
  if(nbqGetDir(dirname,brainTerm)<0){
    nbLogMsgI(0,'E',"Unable to send queue for brain %s at this time.",brainTerm->word->value);
    return;
    }
  if((qHandle=nbQueueOpenDir(dirname,brain->myId,1))==NULL){
    nbLogMsgI(0,'E',"Unable to send queue for brain %s at this time.",brainTerm->word->value);
    return;
    }
  if(trace) nbLogMsgI(0,'T',"nbqSend() %s -> %s:%s",qHandle->filename,brainTerm->word->value,dstQueBrainName);
  while((qEntry=qHandle->entry)!=NULL){
    status=nbQueueOpenFile(qHandle);
    if(trace) nbLogMsgI(0,'T',"nbqSend() file=%s",qHandle->filename);
    if(status<0) nbLogMsgI(0,'E',"Unable to open queue file %s",qHandle->filename);
    else if(status==0) nbLogMsgI(0,'W',"Busy queue file %s",qHandle->filename);
    else if(qHandle->eof==0){
      /* delete empty files */
      if(remove(qHandle->filename)<0) nbLogMsgI(0,'L',"Remove failed on empty queue fil %s",qHandle->filename);
      }
    else if(qEntry->type=='q' && direct){       /* send commands to peer */
      if(nbqSendDirect(qHandle,brainTerm)!=0){  /* give up if we can't communicate with the peer */
        nbQueueCloseDir(qHandle);
        return;
        }
      }
    else{
      if(session==NULL && (session=nbpOpen(nbp,brainTerm,"@"))==NULL){   /* request skull brain */
        nbLogMsgI(0,'E',"nbpOpenTran: Unable to connect to skull peer brain");
        nbQueueCloseDir(qHandle);
        return;
        }
      if(trace) nbLogMsgI(0,'T',"nbqSend() calling nbqSendFile()");
      nbqSendFile(qHandle,session,dstQueBrainName);
      }
    qHandle->entry=qEntry->next;
    nbFree(qEntry,sizeof(struct NBQ_ENTRY));
    }
  if(trace) nbLogMsgI(0,'T',"nbqSend() done processing queue");
  if(session!=NULL){
    nbpStop(session);
    nbpClose(session);
    }
  nbQueueCloseDir(qHandle);
  }
