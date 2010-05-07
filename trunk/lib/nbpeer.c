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
* Program: NodeBrain
*
* File:    nbpeer.c
*
* Title:   NodeBrain Peer Communication Handler
*
* Purpose:
*
*   This file provides functions supporting non-blocking socket
*   communication between peer nodes.  It is intended for situations
*   where data may flow in both directions as asynchronous messages.
*   This API has no clue what the data is, only that it is organized
*   as variable length records.  Higher level API's can apply
*   structure to the records.
*
* Synopse:
*
*   nbPeer *nbPeerConstruct(nbCELL context,char *uri,nbCELL tlsContext,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len));
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   nbPeer *nbPeerModify(nbCELL context,nbPeer *peer,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len));
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   nbPeer *nbPeerListen(nbCELL context,nbPeer *peer);
*   int nbPeerConnect(nbCELL context,nbPeer *peer,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle,int code),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   int nbPeerSend(nbCELL context,nbPeer *peer,void *data,int len);
*   int nbPeerShutdown(nbCELL context,nbPeer *peer,code);
*   int nbPeerDestroy(nbCELL context,nbPeer *peer);
*
*
* NOTE:
*
*   The nbPeerConsumer and nbPeerProducer functions have been modified to manage
*   the nbListener wait conditions. We assume that nbPeerReader and nbPeerWriter
*   are the only listener exits we want to use once the first exit has been taken.
*
* Description:
*
*   This API provides a simple layer on top of the NodeBrain Medulla API and
*   NodeBrain TLS API to exchange data blocks up to 64KB using non-blocking
*   IO via encrypted and authenticated connection.  
*
*   The Medulla API manages when sockets are ready for read or write.  This
*   file provides handlers for the Medulla API.
*
*   The TLS API manages encryption and a layer of authentication and
*   authorization.  The TLS layer is configured using a NodeBrain context,
*   so the NodeBrain rule author has the ability to configure it.
*
*   Two user provided handler functions are required.
*
*     producer - Called when
*                  a) connection has been accepted by listening peer
*                  b) connection has been accomplished after nbPeerConnect call
*                  c) the buffer has been written 
*
*                Code:
*                  -1 - sorry man, things didn't work out with the connection
*                   0 - everything is fine
*                  
*                Returns:
*                  -1 - shutdown the connection
*                   0 - data provided (or not - buffer is checked)
*                  
*     consumer - Called for every record received from the peer
*
*                Returns:
*                  -1 - shutdown the connection
*                   0 - data handled
*
*   The consumer is passed the data as originally sent to nbPeerSend.
*   There should be no assumption that a 2 byte length field is available
*   ahead of the data.  Protocols requiring a length prefix should include
*   their own prefix.  This may seem redundant, but it enables modifications
*   here without impacting applications.
*
*   A void pointer called "handle" is provided to these functions for use
*   as a session handle. The user's notion of a protocol state can be
*   managed as an attribute in the session handle, or by updating the
*   producer and consumer handlers.
*
*   We are focused here on supporting asynchronous non-blocking data flow.
*   However, the functions may be enhancement in the future to support
*   blocking by setting the producer and/or consumer to null values and
*   including an nbPeerRecv function.  We will look at the requirements
*   of an enhanced "peer" module at that time.  For now we are addressing
*   the requirements of the "message" module.
*
*==============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010/01/07 Ed Trettevik (original prototype)
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
*==============================================================================
*/
#include <nb.h>

/*********************************************************************
* Medulla Event Handlers
*********************************************************************/

/*
*  Write data to peer
*/ 
static void nbPeerWriter(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int len;
  size_t size;
  int code;

  nbLogMsg(context,0,'T',"nbPeerWriter: called for sd=%d",sd);
  size=peer->wloc-peer->wbuf;
  nbLogMsg(context,0,'T',"nbPeerWriter: called for sd=%d size=%d",sd,size);
  if(size){
    //nbListenerRemoveWrite(context,sd);
    //peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    //return;
    //}
    len=nbTlsWrite(peer->tls,(char *)peer->wbuf,size);
    if(len<0){
      nbLogMsg(context,0,'E',"nbPeerWriter: nbTlsWrite failed - %s",strerror(errno));
      peer->flags|=NB_PEER_FLAG_WRITE_ERROR;
      }
    if(len<size){
      size-=len;
      memcpy(peer->wbuf,peer->wloc+len,size);
      peer->wloc=peer->wbuf+size;
      } 
    else peer->wloc=peer->wbuf;
    }
  if(peer->producer){
    if((code=(*peer->producer)(context,peer,peer->handle))){  // call producer for more data
      nbPeerShutdown(context,peer,code);
      }
    if(peer->wloc==peer->wbuf){  // if we don't have more data to write, stop waiting to write
      nbListenerRemoveWrite(context,sd);
      peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else{
    nbListenerRemoveWrite(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  }

/*
*  Read data from peer
*/
static void nbPeerReader(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int len;
  size_t size;
  nbTLS *tls=peer->tls;
  unsigned char *bufcur,*dataend;
  int code;
  
  nbLogMsg(context,0,'T',"nbPeerReader: called for sd=%d",sd);
  if(!peer->consumer){  // this should not happen
    nbLogMsg(context,0,'T',"nbPeerReader: data available but no consumer - removing wait");
    nbListenerRemove(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  size=NB_PEER_BUFLEN-(peer->rloc-peer->rbuf);
  len=nbTlsRead(tls,(char *)peer->rloc,size);
  if(len<=0){
    if(len==0) nbLogMsg(context,0,'I',"nbPeerReader: Peer %d %s has shutdown connection",sd,tls->uriMap[tls->uriIndex].uri);
    else nbLogMsg(context,0,'E',"nbPeerReader: Peer %d %s unable to read - %s",sd,tls->uriMap[tls->uriIndex].uri,strerror(errno));
    peer->flags|=NB_PEER_FLAG_WRITE_ERROR;
    if(errno==EINPROGRESS){ // check for unexpected in progress state
      nbLogMsg(context,0,'L',"nbPeerReader: Fatal error - got here because socket was read to read but now in progress?");
      exit(1);
      }
    // Let shutdown handle the shutdown
    //nbListenerRemove(context,sd);
    //nbTlsClose(tls);
    //peer->tls=NULL;
    peer->flags|=NB_PEER_FLAG_WRITE_ERROR;
    if(len==0) nbPeerShutdown(context,peer,0);
    else nbPeerShutdown(context,peer,-1);
    return;
    } 
  // have to deblock the messages here
  dataend=peer->rloc+len;
  bufcur=peer->rbuf;

  while(bufcur<dataend && peer->consumer){
    if(dataend-peer->rbuf<2){
      nbLogMsg(context,0,'T',"nbPeerReader: message length is split - have to read again");
      peer->rloc+=len;
      if(bufcur!=peer->rbuf){
        memcpy(peer->rbuf,bufcur,peer->rloc-bufcur);
        peer->rloc-=bufcur-peer->rbuf;
        if(peer->rloc<peer->rbuf){
           nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal logic error - rloc %p < rbuf %p",sd,tls->uriMap[tls->uriIndex].uri,peer->rloc,peer->rbuf);
           exit(1);
           }
        }
      return;
      }
    len=(*bufcur<<8)|*(bufcur+1);
    if(bufcur+len>dataend){
      nbLogMsg(context,0,'T',"nbPeerReader: didn't get full message - have to read again");
      peer->rloc=dataend;
      if(bufcur!=peer->rbuf){
        memcpy(peer->rbuf,bufcur,dataend-bufcur);
        peer->rloc-=bufcur-peer->rbuf;
        if(peer->rloc<peer->rbuf){
           nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal logic error - rloc %p < rbuf %p",sd,tls->uriMap[tls->uriIndex].uri,peer->rloc,peer->rbuf);
           exit(1);
           }
        }
      return;
      }
    // call the consumer
    nbLogMsg(context,0,'T',"nbPeerReader: calling the consumer exit");
    if((code=(*peer->consumer)(context,peer,peer->handle,bufcur+2,len-2))){
      nbLogMsg(context,0,'T',"nbPeerReader: Peer %d %s shutting down by consumer request",sd,tls->uriMap[tls->uriIndex].uri);
      nbListenerRemove(context,sd); // shutdown should do this for us
      peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
      nbPeerShutdown(context,peer,code);
      return;
      }
    bufcur+=len;
    //nbLogMsg(context,0,'T',"nbPeerReader: new bufcur=%p",bufcur);
    }
  if(peer->consumer){
    if(bufcur!=dataend){
      nbLogMsg(context,0,'T',"nbPeerReader: Peer %d %s fatal error - didn't finish at end of data - terminating",sd,tls->uriMap[tls->uriIndex].uri);
      exit(1);
      }
    peer->rloc=peer->rbuf;  // next read will be at start of buffer
    }
  else{ // the consumer has been cancelled in the middle of reading
    nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal defect - consumer bailed in middle of sending buffer - terminating",sd,tls->uriMap[tls->uriIndex].uri);
    exit(1);
    }
  nbLogMsg(context,0,'T',"nbPeerReader: returning");
  }


/*
*  Call producer after TLS handshake is complete 
*
*    It doesn't seem necessary to call the producer here.
*    Can't we just let the nbPeerWriter routine call it
*    and respond?  Need to walk through this.
*/
static void nbPeerConnecter(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int code;

  nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x",sd,peer->flags);
  if(peer->producer){
    if((code=(*peer->producer)(context,peer,peer->handle))){
      nbPeerShutdown(context,peer,code);
      return;
      }
    if(peer->wloc>peer->wbuf) nbPeerWriter(context,sd,handle);
    if(peer->producer){
      nbListenerAddWrite(context,sd,peer,nbPeerWriter);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x has no producer",sd,peer->flags);
  if(peer->consumer){
    nbListenerAdd(context,sd,peer,nbPeerReader);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  else nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x has no consumer",sd,peer->flags);
  nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x",sd,peer->flags);
  }

static void nbPeerHandshakeReader(nbCELL context,int sd,void *handle);
/*
*  Handle TLS handshake when OpenSSL wants to write 
*
*    We should not get here without a tls pointer.
*/
static void nbPeerHandshakeWriter(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int rc;

  nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: Peer %d flags=%x",sd,peer->flags);
  if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
    nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: ready after CONNECTING_WRITE_WAIT");
    nbListenerRemoveWrite(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  if(!peer->tls->tlsx){
    nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: handing off to nbPeerConnecter 1");
    nbListenerAddWrite(context,sd,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    return;
    }
  // 2010-02-03 eat - this was commented out, but I'm trying to get it working
  //if(peer->tls->option && !peer->tls->ssl){
  //  nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: calling nbTlsConnected");
  //  if(nbTlsConnected(peer->tls)<0){
  //    nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: Unable to set ssl  - terminating");
  //    exit(1);
  //    }
  //  }
  if(!peer->tls->option || (rc=nbTlsHandshakeNonBlocking(peer->tls))==0){
    nbLogMsg(context,0,'T',"nbPeerHandshakeWriter: handing off to nbPeerConnecter 2");
    nbListenerAddWrite(context,sd,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    return;
    }
  else if(rc==SSL_ERROR_WANT_WRITE){
    nbListenerAddWrite(context,sd,peer,nbPeerHandshakeWriter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_READ){
    nbListenerAdd(context,sd,peer,nbPeerHandshakeReader);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  else nbPeerShutdown(context,peer,-1);
  }

/*
*  Handle TLS handshake when OpenSSL wants to read 
*/
static void nbPeerHandshakeReader(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int rc;

  nbLogMsg(context,0,'T',"nbPeerHandshakeReader: called");
  nbListenerRemove(context,sd);
  peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
  if((rc=nbTlsHandshakeNonBlocking(peer->tls))==0){
    nbListenerAddWrite(context,sd,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_WRITE){
    nbListenerAddWrite(context,sd,peer,nbPeerHandshakeWriter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_READ){
    nbListenerAdd(context,sd,peer,nbPeerHandshakeReader);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  else nbPeerShutdown(context,peer,-1);
  }

/*
*  Accept a connection and call the producer
*
*    It is typical for the producer of a listening peer to
*    provide a new producer and handle when called.  The
*    initial producer may handle the exchange required to
*    construct or locate a new handle.
*/
static void nbPeerAccepter(nbCELL context,int sd,void *handle){
  nbPeer *peer,*lpeer=(nbPeer *)handle;
  nbTLS *tls;

  nbLogMsg(context,0,'T',"nbPeerAccepter: called for sd=%d",sd);
  tls=nbTlsAccept(lpeer->tls);
  if(!tls){
    nbLogMsg(context,0,'T',"nbPeerAccepter: nbTlsAccept failed");
    nbListenerRemove(context,sd);
    lpeer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  nbLogMsg(context,0,'T',"nbPeerAccepter: nbTlsAccept succeeded");
  peer=(nbPeer *)nbAlloc(sizeof(nbPeer));
  memcpy(peer,lpeer,sizeof(nbPeer));
  peer->tls=tls;
  if(!peer->wbuf) peer->wbuf=(unsigned char *)malloc(NB_PEER_BUFLEN);
  peer->wloc=peer->wbuf;
  if(!peer->rbuf) peer->rbuf=(unsigned char *)malloc(NB_PEER_BUFLEN);
  peer->rloc=peer->rbuf;
  // Should not call producer until we get to nbPeerWriter
  //nbLogMsg(context,0,'T',"nbPeerAccepter: calling handler=%p",peer->producer);
  //if((*peer->producer)(context,peer,peer->handle)<0){
  //  nbLogMsg(context,0,'T',"nbPeerAccepter: handler want's to bail");
  //  nbPeerDestroy(context,peer);
  //  }
  if(tls->option==NB_TLS_OPTION_TCP){
    nbListenerAdd(context,tls->socket,peer,nbPeerReader);
    nbListenerAddWrite(context,tls->socket,peer,nbPeerWriter);
    }
  else{
    nbListenerAdd(context,tls->socket,peer,nbPeerHandshakeReader);
    nbListenerAddWrite(context,tls->socket,peer,nbPeerHandshakeWriter);
    }
  peer->flags|=NB_PEER_FLAG_WRITE_WAIT|NB_PEER_FLAG_READ_WAIT;
  nbLogMsg(context,0,'T',"nbPeerAccepter: returning");
  }


/*********************************************************************
* API - Functions below are API functions.  Those above are internal.
*********************************************************************/

/*
*  Create a peer structure for listening or connecting.
*
*    Buffers are not allocated until a connection is established.
*/
nbPeer *nbPeerConstruct(nbCELL context,char *uriName,char *uri,nbCELL tlsContext,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  nbPeer *peer;
  nbTLSX  *tlsx;

  nbLogMsg(context,0,'T',"nbPeerConstruct: called uri=%s",uri);
  // allocate a peer structure
  peer=(nbPeer *)nbAlloc(sizeof(nbPeer));
  memset(peer,0,sizeof(nbPeer));
  peer->handle=handle;
  peer->producer=producer;
  peer->consumer=consumer;
  peer->shutdown=shutdown;
  uri=nbTermOptionString(tlsContext,uriName,uri);
  nbLogMsg(context,0,'T',"nbPeerConstruct: configured uri=%s",uri);
  tlsx=nbTlsLoadContext(context,tlsContext,peer);
  peer->tls=nbTlsCreate(tlsx,uri);
  if(peer->tls) nbLogMsg(context,0,'T',"nbPeerCreate: called uri=%s",uri);
  else nbLogMsg(context,0,'T',"nbPeerConstruct: unable to create tls for uri=%s",uri);
  return(peer);
  }

/*
*  Start listening as a peer
*
*    The peer structure is cloned when a connection is accepted
*    and the producer is invoked.  The producer may then call other
*    API functions to change the handle, producer, or consumer, and
*    may also call nbPeerSend.  The peer structure created by nbPeerListen
*    does not have allocated buffers.
*    
*/
int nbPeerListen(nbCELL context,nbPeer *peer){
  nbLogMsg(context,0,'T',"nbPeerListen: called uri=%s",peer->tls->uriMap[0].uri);
  if(nbTlsListen(peer->tls)<0){
    nbLogMsg(context,0,'E',"Unable to listener - %s",peer->tls->uriMap[0].uri);
    return(-1);
    }
  fcntl(peer->tls->socket,F_SETFD,fcntl(peer->tls->socket,F_GETFD)|O_NONBLOCK);
  nbListenerAdd(context,peer->tls->socket,peer,nbPeerAccepter);
  peer->flags|=NB_PEER_FLAG_READ_WAIT;
  nbLogMsg(context,0,'T',"nbPeerListen: things look good");
  return(0);
  }

/*
*  Establish a connection with a peer
*
*    Buffers are allocated under the assumption that the connection will
*    be successful, now or in the future.  The buffers are freed when
*    a peer is shutdown or destroyed.
*/
int nbPeerConnect(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  int rc;

  nbLogMsg(context,0,'T',"nbPeerConnect: called uri=%s",peer->tls->uriMap[0].uri);
  if(!peer->wbuf) peer->wbuf=(unsigned char *)malloc(NB_PEER_BUFLEN);
  peer->wloc=peer->wbuf;
  if(!peer->rbuf) peer->rbuf=(unsigned char *)malloc(NB_PEER_BUFLEN);
  peer->rloc=peer->rbuf;
  peer->handle=handle;
  peer->producer=producer;
  peer->consumer=consumer;
  peer->shutdown=shutdown;
  rc=nbTlsConnectNonBlockingAndSchedule(context,peer->tls,peer,nbPeerHandshakeWriter);
  if(rc<0){
    nbLogMsg(context,0,'E',"nbPeerConnect: Unable to connect - %s",strerror(errno));
    nbPeerShutdown(context,peer,-1);
    return(-1);
    }
  nbLogMsg(context,0,'T',"nbPeerConnect: returning - good luck waiting for a connection");
  return(0);
  }

/*
*  Buffer data up for peer and send as soon as possible
*
*  Returns:
*
*    -1 - error encountered - need to shutdown
*     0 - success
*     1 - buffer is full - will call producer when ready for retry
*/
int nbPeerSend(nbCELL context,nbPeer *peer,void *data,int size){
  int mysize=size+2;
  nbLogMsg(context,0,'T',"nbPeerSend: called with peer=%p size=%d flags=%x",peer,size,peer->flags);
  nbLogMsg(context,0,'T',"nbPeerSend: peer->wloc=%p",peer->wloc);
  nbLogMsg(context,0,'T',"nbPeerSend: peer->wbuf=%p",peer->wbuf);
  if(peer->flags&NB_PEER_FLAG_WRITE_ERROR) return(-1);
  nbLogMsg(context,0,'T',"nbPeerSend: after flag check");
  if(size>NB_PEER_BUFLEN-2) return(-1);
  nbLogMsg(context,0,'T',"nbPeerSend: after length check");
  if(peer->wloc+size+2>peer->wbuf+NB_PEER_BUFLEN) return(1);
  nbLogMsg(context,0,'T',"nbPeerSend: after full buffer check");
  // put message in buffer
  memcpy(peer->wloc+2,data,size);
  *peer->wloc=mysize>>8;
  peer->wloc++;
  *peer->wloc=mysize&0xff;
  peer->wloc++;
  peer->wloc+=size;
  nbLogMsg(context,0,'T',"nbPeerSend: inserting - recsize=%d wbuf size=%d",size,peer->wloc-peer->wbuf);

  if(!(peer->flags&NB_PEER_FLAG_WRITE_WAIT)){
    nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerWriter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  return(0);
  }

void nbPeerModify(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  peer->handle=handle;

  peer->producer=producer;
  if(!producer){
    if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
      nbListenerRemoveWrite(context,peer->tls->socket);
      peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else{
    if(!(peer->flags&NB_PEER_FLAG_WRITE_WAIT)){
      nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerWriter);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      }
    }

  peer->consumer=consumer; 
  if(!consumer){
    if(peer->flags&NB_PEER_FLAG_READ_WAIT){
      nbListenerRemove(context,peer->tls->socket);
      peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
      }
    }
  else{
    if(!(peer->flags&NB_PEER_FLAG_READ_WAIT)){
      nbListenerAdd(context,peer->tls->socket,peer,nbPeerReader);
      peer->flags|=NB_PEER_FLAG_READ_WAIT;
      }
    }

  peer->shutdown=shutdown;
  }

/*
*  Shutdown peer connection
*
*    If it would be helpful for the producer and consumer to receive
*    a reason for the shutdown, there a couple options.  The caller
*    could set a code in the peer structure before calling nbPeerShutdown,
*    or an additional int parameter can be added to nbPeerShutdown.
*/
int nbPeerShutdown(nbCELL context,nbPeer *peer,int code){
  nbLogMsg(context,0,'T',"nbPeerShutdown: %s code=%d",peer->tls->uriMap[peer->tls->uriIndex].uri,code);
  if(peer->shutdown) (*peer->shutdown)(context,peer,peer->handle,code);
  if(peer->tls) nbTlsClose(peer->tls);
  if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,peer->tls->socket);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  if(peer->flags&NB_PEER_FLAG_READ_WAIT){
    nbListenerRemove(context,peer->tls->socket);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  peer->flags&=0xff-NB_PEER_FLAG_WRITE_ERROR;
  if(peer->wbuf) free(peer->wbuf);
  peer->wbuf=NULL;
  if(peer->rbuf) free(peer->rbuf);
  peer->rbuf=NULL;
  peer->producer=NULL;
  peer->consumer=NULL;
  peer->shutdown=NULL;
  return(0);
  }

int nbPeerDestroy(nbCELL context,nbPeer *peer){
  nbLogMsg(context,0,'T',"nbPeerDestroy: called");
  if(peer->tls) nbLogMsg(context,0,'T',"nbPeerDestroy: uri=%s",peer->tls->uriMap[0].uri);
  if(peer->tls) nbTlsFree(peer->tls);
  if(peer->wbuf) free(peer->wbuf);
  if(peer->rbuf) free(peer->rbuf);
  nbFree(peer,sizeof(nbPeer));
  return(0);
  }

