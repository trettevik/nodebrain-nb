/*
* Copyright (C) 1998-2013 The Boeing Company
* Copyright (C) 2014      Ed Trettevik <eat@nodebrain.org>
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
* File:    nbproxy.c
*
* Title:   NodeBrain Proxy Communication Handler
*
* Purpose:
*
*   This file provides functions supporting non-blocking socket
*   communication between proxy nodes.  It is intended for situations
*   where data may flow in both directions as asynchronous messages.
*   This API has no clue what the data is, and leaves it up to higher
*   level API's to make sense of it.
*    
*   These routines are very similar to the API provide by nbpeer.c.
*   However, this API handles data in pages, while the Peer API handles
*   data as variable length records.  This API doesn't move data as much in
*   in memory because it passes page pointers instead of the data.
*
*   A "proxy" here is a mechanism for exchanging information with a "peer".
*   An actual proxy service is implemented with two "proxies" with service
*   functionality implimented between them.
*
* Synopsis:
*
*   nbProxy *nbProxyConstruct(nbCELL context,char *uri,nbCELL tlsContext,void *handle,
*     int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
*     int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
*     void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));
*   nbProxy *nbProxyModify(nbCELL context,nbProxy *proxy,void *handle,
*     int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
*     int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
*     void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));
*   nbProxy *nbProxyListen(nbCELL context,nbProxy *proxy);
*   int nbProxyConnect(nbCELL context,nbProxy *proxy,void *handle,
*     int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
*     int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
*     void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));
*   int nbProxySend(nbCELL context,nbProxy *proxy,void *data,int len);
*   int nbProxyShutdown(nbCELL context,nbProxy *proxy,code);
*   int nbProxyDestroy(nbCELL context,nbProxy *proxy);
*
* NOTE:
*
*   The nbProxyConsumer and nbProxyProducer functions have been modified to manage
*   the nbListener wait conditions. We assume that nbProxyReader and nbProxyWriter
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
*                  a) connection has been accepted by listening proxy
*                  b) connection has been accomplished after nbProxyConnect call
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
*     consumer - Called for every record received from the proxy
*
*                Returns:
*                  -1 - shutdown the connection
*                   0 - data handled
*
*   The consumer is passed the data as originally sent to nbProxySend.
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
*   including an nbProxyRecv function.  We will look at the requirements
*   of an enhanced "proxy" module at that time.  For now we are addressing
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
* 2010/06/06 eat 0.8.2  Added client parameter to nbTlsLoadContext parameter
* 2011-02-08 eat 0.8.5  Refined non-blocking SSL handshake
* 2011-05-14 eat 0.8.5  Included notion of a book of pages
* 2012-05-11 eat 0.8.9  Included NB_PROXY_FLAG_FINISH_OUTPUT/INPUT flag to shutdown after consumption
* 2012-05-12 eat 0.8.9  Modified nbProxyForwardProducer to return 2 when other proxy socket is closed
*            The idea here is to continue forwarding data back from the server
*            even after the server has closed the socket.  This is behavior
*            appropriate for a web server (and probably others) but not all
*            forwarding situations, so it is an option determined by the
*            NB_PROXY_FLAG_FINISH_INPUT flag.
* 2012-05-16 eat 0.8.9  Changed nbProxyConnect to accept TLSX and URI as parameters
*            Previously the TLSX and URI values were constructed on each call to
*            nbProxyConnect, adding overhead and a memory leak impacting high
*            frequency callers.
* 2012-05-19 eat 0.8.9  Included connection failover
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-27 eat 0.8.13 Checker updates
* 2012-12-30 eat 0.8.13 Modified nbProxyConstuct to only load context for secure protocols
* 2014-02-16 eat 0.6.18 Optional TLS
*==============================================================================
*/
#include "../config.h"
#ifdef HAVE_OPENSSL

#include <nb/nb.h>
#include <nb/nbproxy.h>  // will move to nb.h after it settles down

int proxyTrace;          // debugging trace flag for proxy routines

/*********************************************************************
* Proxy page functions
*********************************************************************/
static nbProxyPage *nb_proxy_page=NULL;  // list of pages

nbProxyPage *nbProxyPageOpen(nbCELL context,void **data,size_t *size){
  nbProxyPage *page;

  if((page=nb_proxy_page)==NULL){
    page=(nbProxyPage *)nbAlloc(sizeof(nbProxyPage)); 
    page->data=nbAlloc(NB_PROXY_PAGESIZE);
    page->size=NB_PROXY_PAGESIZE;
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPageOpen: page=%p allocated",page);
    }
  else{
    nb_proxy_page=nb_proxy_page->next;
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPageOpen: page=%p reused",page);
    }
  page->next=NULL;
  page->dataLen=0;
  page->flags=0;
  *data=page->data;
  *size=page->size;
  return(page);
  }

int nbProxyPageProduced(nbCELL context,nbProxyPage *page,int len){
  int unwritten=page->size-page->dataLen;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPageProduced: called len=%d unwritten=%d",len,unwritten);
  if(len>unwritten){
    nbLogMsg(context,0,'L',"nbProxyPageProduced: claim to have produced more than available space - terminating");
    exit(1); 
    }
  page->dataLen+=len;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPageProduced: returning dataLen=%d",page->dataLen);
  return(0);
  }

void *nbProxyPageClose(nbCELL context,nbProxyPage *page){
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPageClose: page=%p",page);
  page->next=nb_proxy_page;
  nb_proxy_page=page;
  return(NULL);
  }

/*********************************************************************
* Proxy book functions
*********************************************************************/

nbProxyBook *nbProxyBookOpen(nbCELL context){
  nbProxyBook *book;
  book=(nbProxyBook *)nbAlloc(sizeof(nbProxyBook));
  memset(book,0,sizeof(nbProxyBook));
  return(book);
  }

// Note: this function should not be called with a cached page
//       We'll need a change here or different function for cached pages
int nbProxyBookPutPage(nbCELL context,nbProxyBook *book,nbProxyPage *page){
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: called");
  if(!book->writePage){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: no writePage - readPage=%p",book->readPage);
    book->writePage=page;
    if(!book->readPage) book->readPage=page;
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: returning with book->writePage=%p book->readPage=%p",book->writePage,book->readPage);
    return(0);
    }
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: A book->writePage=%p book->readPage=%p",book->writePage,book->readPage);
  book->writePage->next=page;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: page->next=%p",page->next);
  while(page->next!=NULL) page=page->next;
  book->writePage=page;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookPutPage: B book->writePage=%p book->readPage=%p",book->writePage,book->readPage);
  if(book->readPage!=book->writePage) return(1);  // have more than one page
  return(0);  // have only one page
  }

nbProxyPage *nbProxyBookGetPage(nbCELL context,nbProxyBook *book){
  nbProxyPage *page=book->readPage;
  if(page){
    book->readPage=page->next;
    if(!book->readPage) book->writePage=NULL; // 2011-06-03 - eat
    }
  return(page);
  }

int nbProxyBookWriteWhere(nbCELL context,nbProxyBook *book,void **data){
  nbProxyPage *page;
  size_t len;

  //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: called data=%p avail=%d",*data,len);
  if(book->readPage==NULL){
    //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: starting first page");
    book->readPage=nbProxyPageOpen(context,data,&len);
    book->writePage=book->readPage;
    }
  else{
    len=book->writePage->size-book->writePage->dataLen;
    //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: page=%p size=%d used=%d avail=%d",book->writePage->data,book->writePage->size,book->writePage->dataLen,len);
    if(len==0){
      //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: starting new page");
      page=nbProxyPageOpen(context,data,&len);
      book->writePage->next=page; 
      book->writePage=page;
      }
    else{
      //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: using existing page");
      *data=((char *)book->writePage->data)+book->writePage->dataLen;
      }
    }
  //nbLogMsg(context,0,'T',"nbProxyBookWriteWhere: data=%p avail=%d",*data,len);
  return(len);
  }

int nbProxyBookProduced(nbCELL context,nbProxyBook *book,int len){
  nbProxyPage *page=book->writePage;
  int unwritten;
  if(len==0) return(0);
  if(!page) return(-1);
  unwritten=page->size-page->dataLen;
  if(len>unwritten){
    nbLogMsg(context,0,'L',"nbProxyBookProduced: claim to have produced more than available space - terminting");
    exit(1);
    }
  page->dataLen+=len;
  return(0);
  }

int nbProxyBookReadWhere(nbCELL context,nbProxyBook *book,void **data){
  nbProxyPage *page=book->readPage;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookReadWhere: called");
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookReadWhere: page=%p",page);
  if(!page) return(0);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookReadWhere: have page");
  *data=((char *)page->data)+book->readOffset;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyBookReadWhere: dataLen=%d, readOffset=%d",page->dataLen,book->readOffset);
  return(page->dataLen-book->readOffset);
  }

int nbProxyBookConsumed(nbCELL context,nbProxyBook *book,int len){
  nbProxyPage *page=book->readPage;
  int unread=page->dataLen-book->readOffset;
  nbLogMsg(context,0,'T',"nbProxyBookConsumed: called len=%d unread=%d",len,unread);
  if(len>unread){
    nbLogMsg(context,0,'L',"nbProxyBookConsumed: claim to have read more than available - terminating");
    exit(1);
    }
  book->readOffset+=len;
  if(book->readOffset==page->dataLen){
    book->readPage=page->next;
    book->readOffset=0;
    nbProxyPageClose(context,page);
    if(!book->readPage) book->writePage=NULL;
    }
  return(0);
  }

void *nbProxyBookClose(nbCELL context,nbProxyBook *book){
  nbProxyPage *page=book->readPage;
  while(page){
    book->readPage=page->next;
    nbProxyPageClose(context,page);
    page=book->readPage;
    }
  book->writePage=NULL;
  book->readOffset=0;
  return(NULL);
  }

/*********************************************************************
* Medulla Event Handlers
*********************************************************************/

/*
*  Write data to proxy
*
*    A producer may call nbProxyWriteWhere or nbProxyWritePage
*    Producer returns:
*     -1 - shutdown immediately
*      0 - call again after you write some data
*      1 - stop calling until a proxy write function is called
*      2 - stop calling and shutdown when data is consumed
*/ 
static void nbProxyWriter(nbCELL context,int sd,void *handle){
  nbProxy *proxy=(nbProxy *)handle;
  int len;
  int code;
  void *data;
  int   size;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: called for sd=%d",sd);
  size=nbProxyBookReadWhere(context,&proxy->obook,&data);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: called for sd=%d size=%d",sd,size);
  if(size){
    len=nbTlsWrite(proxy->tls,(char *)data,size);
    if(len<0){
      if(proxy->tls->error==NB_TLS_ERROR_WANT_WRITE) return; // will try again later
      if(proxy->tls->error==NB_TLS_ERROR_WANT_READ){
        return; // will try again later
        }
      nbLogMsg(context,0,'E',"nbProxyWriter: nbTlsWrite failed - %s",strerror(errno));
      proxy->flags|=NB_PROXY_FLAG_WRITE_ERROR;
      return;
      }
    if(proxyTrace){
      nbLogMsg(context,0,'T',"nbProxyWriter: output to sd=%d",sd);
      nbLogDump(context,data,len);
      nbLogBar(context);
      }
    nbProxyBookConsumed(context,&proxy->obook,len);
    }
  if(proxy->producer && !(proxy->flags&NB_PROXY_FLAG_PRODUCER_STOP)){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: calling proxy->producer=%p",proxy->producer);
    code=(*proxy->producer)(context,proxy,proxy->handle);  // call producer for more data
    if(code<0){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: producer returned negative code - shutting down");
      nbProxyShutdown(context,proxy,code);
      return; // 2012-05-12 eat
      }
    //else if(code>1){  // 2012-05-11 eat - this looks like an error
    else if(code>0){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: producer returned positive code - disabling producer calls - code=%d",code);
      proxy->flags|=NB_PROXY_FLAG_PRODUCER_STOP;
      if(code==2) proxy->flags|=NB_PROXY_FLAG_FINISH_OUTPUT;
      }
    }
  if(!proxy->obook.readPage){  // if we don't have more data to write, stop waiting to write
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: removing WRITE_WAIT on SD=%d because we have no more data at the moment",sd);
    nbListenerRemoveWrite(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_WRITE_WAIT;
    if(proxy->flags&NB_PROXY_FLAG_FINISH_OUTPUT){ // 2012-05-11 eat - shutdown if time to close half proxy
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: shutting down based on FINISH_OUTPUT flag - sd=%d",sd);
      nbProxyShutdown(context,proxy,0);
      }
    else if(proxy->other && !proxy->other->ibook.readPage && !proxy->other->tls){ // 2012-05-12 eat - shutdown if time to close full proxy
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: shutting down based on other proxy state - sd=%d",sd);
      nbProxyShutdown(context,proxy,0);
      }
    if(proxy->other) nbLogMsg(context,0,'T',"nbProxyWriter: proxy->other->tls==%p",proxy->other->tls);
    }
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyWriter: returning",sd);
  }

/*
*  Read data from proxy
*
*    A consumer may call nbProxyReadWhere or nbProxyReadPage to get data.
*   
*    Consumer returns:
*     -1 - shutdown
*      0 - call again when you read more data
*      1 - stop calling until a proxy read function is called
*/
static void nbProxyReader(nbCELL context,int sd,void *handle){
  nbProxy *proxy=(nbProxy *)handle;
  int len;
  size_t size;
  void *data;
  nbTLS *tls=proxy->tls;
  int code;
  
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: called for sd=%d",sd);
  size=nbProxyBookWriteWhere(context,&proxy->ibook,&data);
  len=nbTlsRead(tls,(char *)data,size);
  if(len<0){
    if(tls->error==NB_TLS_ERROR_WANT_READ) return; // will try again later
    if(tls->error==NB_TLS_ERROR_WANT_WRITE){
      return; // will try again later
      }
    nbLogMsg(context,0,'E',"nbProxyReader: Proxy %d %s unable to read - %s",sd,tls->uriMap[tls->uriIndex].uri,strerror(errno));
    proxy->flags|=NB_PROXY_FLAG_WRITE_ERROR;
    nbProxyShutdown(context,proxy,-1);
    return;
    }
  if(len==0){
    nbLogMsg(context,0,'I',"nbProxyReader: Proxy %d %s has shutdown connection",sd,tls->uriMap[tls->uriIndex].uri);
    proxy->flags|=NB_PROXY_FLAG_WRITE_ERROR;
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: peer closed %d",sd);
    if(proxy->flags&NB_PROXY_FLAG_FINISH_INPUT){ // 2012-05-13 eat - if we need to finish input process after close
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: have we processed all input from %d",sd);
      if((!proxy->ibook.readOffset && proxy->ibook.readPage==proxy->ibook.writePage) && (!proxy->other || !(proxy->other->flags&NB_PROXY_FLAG_WRITE_WAIT))){
        nbProxyShutdown(context,proxy,0); // If this ibook is empty we have no other half or the other half is empty too, just shutdown
        return;
        }
      if(proxyTrace){
        nbLogMsg(context,0,'T',"nbProxyReader: no we still have unprocessed input from %d",sd);
        nbLogMsg(context,0,'T',"nbProxyReader: proxy->ibook.readPage=%p",proxy->ibook.readPage);
        nbLogMsg(context,0,'T',"nbProxyReader: proxy->other=%p",proxy->other);
        }
      if(proxy->other) nbLogMsg(context,0,'T',"nbProxyReader: proxy->other->flags=%2.2x",proxy->other->flags);
      nbListenerRemove(context,sd);
      proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
      nbTlsClose(tls);
      proxy->tls=NULL;
      nbProxyBookClose(context,&proxy->ibook);
      return;
      }
    else  nbProxyShutdown(context,proxy,0);
    return;
    } 
  if(proxyTrace){
     nbLogMsg(context,0,'T',"nbProxyReader: input from sd=%d",sd);
     nbLogDump(context,data,len);
     nbLogBar(context);
     }
  nbProxyBookProduced(context,&proxy->ibook,len);
  if(proxy->consumer && !(proxy->flags&NB_PROXY_FLAG_CONSUMER_STOP)){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: calling proxy->consumer=%p",proxy->consumer);
    code=(*proxy->consumer)(context,proxy,proxy->handle);
    if(code<0){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: consumer returned negative code - shutting down");
      nbProxyShutdown(context,proxy,code);
      return;
      }
    else if(code>0){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: consumer returned positive code - disabling consumer calls");
      proxy->flags|=NB_PROXY_FLAG_CONSUMER_STOP;
      }
    }
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: ibook.readPage=%p ibook.writePage=%p",proxy->ibook.readPage,proxy->ibook.writePage);
  if(proxy->ibook.readPage!=proxy->ibook.writePage){  // stop reading on full buffer
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: removing read wait %d %s",sd,nbTlsGetUri(tls));
    nbListenerRemove(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
    return;
    }
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyReader: returning");
  }

static int nbProxyForwardProducer(nbCELL context,nbProxy *proxy,void *handle){
  nbProxyPage *page;

  nbLogMsg(context,0,'T',"nbProxyForwardProducer: called proxy=%p other=%p",proxy,proxy->other);
  page=nbProxyGetPage(context,proxy->other);
  nbLogMsg(context,0,'T',"nbProxyForwardProducer: page=%p from other",page);
  if(!page){
    if(proxy->other->flags&NB_PROXY_FLAG_FINISH_INPUT && !proxy->other->tls) return(2);
    return(1);
    }
  nbLogMsg(context,0,'T',"nbProxyForwardProducer: putting page(s) to self");
  while(page){
    nbProxyPutPage(context,proxy,page);
    page=nbProxyGetPage(context,proxy->other);
    }
  nbLogMsg(context,0,'T',"nbProxyForwardProducer: returning");
  return(0);
  }

static int nbProxyForwardConsumer(nbCELL context,nbProxy *proxy,void *handle){
  nbProxyPage *page;
  int rc;

  nbLogMsg(context,0,'T',"nbProxyForwardConsumer: called");
  page=nbProxyGetPage(context,proxy);
  if(!page) return(0);
  nbLogMsg(context,0,'T',"nbProxyForwardConsumer: have page");
  rc=nbProxyPutPage(context,proxy->other,page);
  nbLogMsg(context,0,'T',"nbProxyForwardConsumer: returning");
  return(0);
  }

static void nbProxyForwardShutdown(nbCELL context,nbProxy *proxy,void *handle,int code){
  nbLogMsg(context,0,'T',"nbProxyForwardShutdown: called");
  if(proxy->other){
    if(proxy->other->other) proxy->other->other=NULL;
    nbProxyShutdown(context,proxy->other,code);
    }
  nbLogMsg(context,0,'T',"nbProxyForwardShutdown: returning");
  }

/*
*  Call producer after TLS handshake is complete 
*/
static void nbProxyConnecter(nbCELL context,int sd,void *handle){
  nbProxy *proxy=(nbProxy *)handle;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnecter: Proxy %d flags=%x",sd,proxy->flags);
  if(proxy->other && !proxy->producer) nbProxyProducer(context,proxy,handle,nbProxyForwardProducer);
  if(proxy->producer){
    nbListenerAddWrite(context,sd,proxy,nbProxyWriter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT){
    nbLogMsg(context,0,'T',"nbProxyConnecter: Proxy %d flags=%x has no producer",sd,proxy->flags);
    nbListenerRemoveWrite(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_WRITE_WAIT;
    }
  if(proxy->consumer){
    nbListenerAdd(context,sd,proxy,nbProxyReader);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  else{
    nbLogMsg(context,0,'T',"nbProxyConnecter: Proxy %d flags=%x has no consumer",sd,proxy->flags);
    nbListenerRemove(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
    }
  nbLogMsg(context,0,'T',"nbProxyConnecter: Proxy %d flags=%x",sd,proxy->flags);
  }

/*
*  Handle TLS handshake when OpenSSL wants read or write during connect 
*/
static void nbProxyConnectHandshaker(nbCELL context,int sd,void *handle){
  nbProxy *proxy=(nbProxy *)handle;
  int rc;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnectHandshaker: called");
  if(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_WRITE_WAIT;
    }
  if(proxy->flags&NB_PROXY_FLAG_READ_WAIT){
    nbListenerRemove(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
    }
  if((rc=nbTlsConnectHandshake(proxy->tls))==0){
    nbLogMsg(context,0,'I',"Proxy connection established with %s",nbTlsGetUri(proxy->tls));
    nbListenerAddWrite(context,sd,proxy,nbProxyConnecter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_WRITE){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnectHandshaker: SSL_ERROR_WANT_WRITE");
    nbListenerAddWrite(context,sd,proxy,nbProxyConnectHandshaker);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_READ){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnectHandshaker: SSL_ERROR_WANT_READ");
    nbListenerAdd(context,sd,proxy,nbProxyConnectHandshaker);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  else{
    nbLogMsg(context,0,'T',"nbProxyConnectHandshaker: flag=%2.2x",proxy->flags);
    if(proxy->flags&NB_PROXY_FLAG_CLIENT){
      while((rc=nbProxyConnectNext(context,proxy))==-1);
      if(rc==0) return;  // give the next uri a try
      }
    nbLogMsg(context,0,'T',"nbProxyConnectHandshaker: handshake failed rc=%d - shutting down",rc);
    nbProxyShutdown(context,proxy,-1);
    }
  }

/*
*  Handle TLS handshake when OpenSSL wants read or write during accept
*/
static void nbProxyAcceptHandshaker(nbCELL context,int sd,void *handle){
  nbProxy *proxy=(nbProxy *)handle;
  int rc;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyAcceptHandshaker: called");
  if(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_WRITE_WAIT;
    }
  if(proxy->flags&NB_PROXY_FLAG_READ_WAIT){
    nbListenerRemove(context,sd);
    proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
    }
  rc=nbTlsAcceptHandshake(proxy->tls);
  if(rc==1){
    nbLogMsg(context,0,'T',"Proxy connection with %s established",nbTlsGetUri(proxy->tls));
    nbListenerAddWrite(context,sd,proxy,nbProxyConnecter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(rc==-1){
    if(proxy->tls->error==NB_TLS_ERROR_WANT_WRITE){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyAcceptHandshaker: SSL_ERROR_WANT_WRITE");
      nbListenerAddWrite(context,sd,proxy,nbProxyAcceptHandshaker);
      proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
      }
    else if(proxy->tls->error==NB_TLS_ERROR_WANT_READ){
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyAcceptHandshaker: SSL_ERROR_WANT_READ");
      nbListenerAdd(context,sd,proxy,nbProxyAcceptHandshaker);
      proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
      }
    else{
      nbLogMsg(context,0,'T',"nbProxyAcceptHandshaker: handshake failed rc=%d - shutting down",rc);
      nbProxyShutdown(context,proxy,-1);
      }
    }
  else{
    nbLogMsg(context,0,'T',"nbProxyAcceptHandshaker: handshake failed rc=%d - shutting down",rc);
    nbProxyShutdown(context,proxy,-1);
    }
  }

/*
*  Accept a connection and call the producer
*
*    It is typical for the producer of a listening proxy to
*    provide a new producer and handle when called.  The
*    initial producer may handle the exchange required to
*    construct or locate a new handle.
*/
static void nbProxyAccepter(nbCELL context,int sd,void *handle){
  nbProxy *proxy,*lproxy=(nbProxy *)handle;
  nbTLS *tls;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyAccepter: called for sd=%d",sd);
  tls=nbTlsAccept(lproxy->tls);
  if(!tls){
    nbLogMsg(context,0,'T',"nbProxyAccepter: nbTlsAccept failed");
    return;
    }
  nbLogMsg(context,0,'T',"nbProxyAccepter: nbTlsAccept succeeded");
  proxy=(nbProxy *)nbAlloc(sizeof(nbProxy));
  memcpy(proxy,lproxy,sizeof(nbProxy));
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyAccepter: proxy->flags=%x",proxy->flags);
  proxy->flags=0;
  proxy->tls=tls;
  // 2011-02-05 eat - this has been reworked for non-blocking IO
  if(tls->option==NB_TLS_OPTION_TCP || tls->error==NB_TLS_ERROR_UNKNOWN){
    nbLogMsg(context,0,'T',"nbProxyAccepter: setting up nbProxyConnecter");
    nbListenerAddWrite(context,tls->socket,proxy,nbProxyConnecter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(tls->error==NB_TLS_ERROR_WANT_WRITE){
    nbListenerAddWrite(context,tls->socket,proxy,nbProxyAcceptHandshaker);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  else if(tls->error==NB_TLS_ERROR_WANT_READ){
    nbListenerAdd(context,tls->socket,proxy,nbProxyAcceptHandshaker);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  nbLogMsg(context,0,'T',"nbProxyAccepter: returning");
  }

/*********************************************************************
* API - Functions below are API functions.  Those above are internal.
*********************************************************************/

/*
*  Create a proxy structure for listening or connecting.
*
*    Buffers are not allocated until a connection is established.
*/
nbProxy *nbProxyConstruct(nbCELL context,int client,nbCELL tlsContext,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code)){

  nbProxy *proxy;
  nbTLSX  *tlsx=NULL;
  char *uri;

  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConstruct: called");
  // allocate a proxy structure
  proxy=(nbProxy *)nbAlloc(sizeof(nbProxy));
  memset(proxy,0,sizeof(nbProxy));
  proxy->handle=handle;
  proxy->producer=producer;
  proxy->consumer=consumer;
  proxy->shutdown=shutdown;
  if(!client){  // for a server go ahead and
    uri=nbTermOptionString(tlsContext,"uri","");
    if(!*uri){
      nbLogMsg(context,0,'E',"nbProxyConstruct: uri not defined in %s",nbTermGetName(context,tlsContext));
      nbFree(proxy,sizeof(nbProxy));
      return(NULL);
      }
    nbLogMsg(context,0,'T',"nbProxyConstruct: uri=%s",uri);
    if(strncmp(uri,"tls:",4)==0 || strncmp(uri,"https:",6)==0) tlsx=nbTlsLoadContext(context,tlsContext,proxy,client);
    proxy->tls=nbTlsCreate(tlsx,uri);
    if(!proxy->tls){
      nbLogMsg(context,0,'E',"nbProxyConstruct: unable to create tls for uri=%s",uri);
      nbFree(proxy,sizeof(nbProxy));
      return(NULL);
      }
    }
  return(proxy);
  }

/*
*  Start listening as a proxy
*
*    The proxy structure is cloned when a connection is accepted
*    and the producer is invoked.  The producer may then call other
*    API functions to change the handle, producer, or consumer, and
*    may also call nbProxySend.  The proxy structure created by nbProxyListen
*    does not have allocated buffers.
*    
*/
int nbProxyListen(nbCELL context,nbProxy *proxy){
  //if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyListen: called uri=%s",proxy->tls->uriMap[0].uri);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyListen: called uri=%s",nbTlsGetUri(proxy->tls));
  if(nbTlsListen(proxy->tls)<0){
    nbLogMsg(context,0,'E',"Unable to listen - %s",nbTlsGetUri(proxy->tls));
    return(-1);
    }
  if(fcntl(proxy->tls->socket,F_SETFL,fcntl(proxy->tls->socket,F_GETFL)|O_NONBLOCK)){ // 2012-12-27 eat 0.8.13 - CID 751524
    nbLogMsg(context,0,'E',"Unable to listen - %s - unable to set non-blocking - %s",nbTlsGetUri(proxy->tls),strerror(errno));
    return(-1);
    }
  nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyAccepter);
  proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyListen: returning - handing off to nbProxyAccepter");
  return(0);
  }

/*
*  Start connection to next uri
*
*  Returns:
*
*   -1 - failed, try next
*    0 - started
*    1 - end of uri values, give up
*/
int nbProxyConnectNext(nbCELL context,nbProxy *proxy){
  int rc;

  nbLogMsg(context,0,'I',"nbProxyConnectNext called after failure with %s",nbTlsGetUri(proxy->tls));
  if(proxy->tls->uriIndex>=proxy->tls->uriCount){
    nbLogMsg(context,0,'W',"nbProxyConnectNext: Unable to connect");
    return(1);
    }
  nbTlsClose(proxy->tls);
  proxy->tls->uriIndex++;  // set to next uri
  rc=nbTlsConnectNonBlocking(proxy->tls);
  switch(rc){
    case 0:
      nbLogMsg(context,0,'I',"Proxy connection established with %s",nbTlsGetUri(proxy->tls));
      nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyConnecter);
      proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
      return(0);
    case 1:
      nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyConnectHandshaker);
      proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
      // fall thru to also wait for read to get notification on errors
    case 2:
      nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyConnectHandshaker);
      proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnectNext: waiting on %s",nbTlsGetUri(proxy->tls));
      return(0);
    }
  // 2012-12-27 eat 0.8.13 - CID 751688
  if(rc<0) nbLogMsg(context,0,'W',"nbProxyConnectNext: Unable to connect %s - %s",nbTlsGetUri(proxy->tls),strerror(errno));
  else nbLogMsg(context,0,'L',"nbProxyConnect: unexpected return code %d from nbTlsConnectNonBlocking");
  return(-1);
  }

/*
*  Establish a connection with a server
*
*    This is a prototype.  Not totally keen on the need for this function
*    to build the nbTLSX and nbTLS structures.  Would be better if we
*    could just clone an nbTLS structure build in advance.  However, this
*    requires some thought to get the TLS/SSL context and handle right.
*    Will look at this more later.
*
*  Returns:
*
*     NULL on error
*     Pointer to proxy on success
*
*/
nbProxy *nbProxyConnect(nbCELL context,nbTLSX *tlsx,char *uri,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code)){

  int rc;
  nbProxy *proxy;

  proxy=(nbProxy *)nbAlloc(sizeof(nbProxy));
  memset(proxy,0,sizeof(nbProxy));
  proxy->tls=nbTlsCreate(tlsx,uri); 
  if(!proxy->tls){
    nbLogMsg(context,0,'E',"nbProxyConnect: unable to create tls handle");
    //nbTlsFreeContext(tlsx);
    nbFree(proxy,sizeof(nbProxy));
    return(NULL);
    }
  nbLogMsg(context,0,'I',"Attempting proxy connection with %s",nbTlsGetUri(proxy->tls));
  proxy->handle=handle;
  proxy->producer=producer;
  proxy->consumer=consumer;
  proxy->shutdown=shutdown;
  proxy->flags|=NB_PROXY_FLAG_CLIENT; // flag as client for failover retries
  rc=nbTlsConnectNonBlocking(proxy->tls);
  switch(rc){
    case 0:
      nbLogMsg(context,0,'I',"Proxy connection established with %s",nbTlsGetUri(proxy->tls));
      nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyConnecter);
      proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
      break; 
    case 1:
      nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyConnectHandshaker);
      proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
      // fall thru to also wait for read to get notification on errors
    case 2:
      nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyConnectHandshaker);
      proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
      if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConnect: waiting on %s",nbTlsGetUri(proxy->tls));
      break; 
    default:
      nbLogMsg(context,0,'W',"nbProxyConnect: Unable to connect to %s - %s",nbTlsGetUri(proxy->tls),strerror(errno));
      while((rc=nbProxyConnectNext(context,proxy))==-1);
      if(rc==1){
        nbProxyShutdown(context,proxy,-1);
        return(NULL);
        }
    }
  return(proxy);
  }

void nbProxyProducer(nbCELL context,nbProxy *proxy,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle)){
  proxy->handle=handle;
  proxy->producer=producer;
  proxy->flags&=0xff-NB_PROXY_FLAG_PRODUCER_STOP;
  if(producer && !(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT)){
    nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyWriter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  }

void nbProxyConsumer(nbCELL context,nbProxy *proxy,void *handle,
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle)){
  nbLogMsg(context,0,'T',"nbProxyConsumer: called");
  proxy->handle=handle;
  proxy->consumer=consumer;
  proxy->flags&=0xff-NB_PROXY_FLAG_CONSUMER_STOP;
  if(consumer && !(proxy->flags&NB_PROXY_FLAG_READ_WAIT)){
    nbLogMsg(context,0,'T',"nbProxyConsumer: setting read wait");
    nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyReader);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  nbLogMsg(context,0,'T',"nbProxyConsumer: returning");
  }

void nbProxyModify(nbCELL context,nbProxy *proxy,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code)){

  proxy->handle=handle;
  nbProxyProducer(context,proxy,handle,producer);
  nbProxyConsumer(context,proxy,handle,consumer);
  proxy->shutdown=shutdown;
  }


/*
*  Join two proxy structures to enable forwarding
*/
void nbProxyForward(nbCELL context,nbProxy *client,nbProxy *server,int option){
  nbLogMsg(context,0,'T',"nbProxyForward: call with option=%x",option);
  client->other=server;
  server->other=client;
  if(option&0x40) client->consumer=nbProxyForwardConsumer;
  if(option&0x20) client->producer=nbProxyForwardProducer;
  if(option&0x10) client->shutdown=nbProxyForwardShutdown;
  if(option&0x04) server->consumer=nbProxyForwardConsumer;
  if(option&0x02) server->producer=nbProxyForwardProducer;
  if(option&0x01) server->shutdown=nbProxyForwardShutdown;
  server->flags|=NB_PROXY_FLAG_FINISH_INPUT;  // finish consuming input event if server closes connection
  }

/*
*  Put page to proxy output book
*
*  Returns:
*
*    -1 - error encountered - need to shutdown
*     0 - success
*     1 - buffer is full - will call producer when ready for retry
*/
int nbProxyPutPage(nbCELL context,nbProxy *proxy,nbProxyPage *page){
  nbLogMsg(context,0,'T',"nbProxyPutPage: proxy=%p page=%p",proxy,page);
  //if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: Sending %d bytes to %s",page->dataLen,nbTlsGetUri(proxy->tls));
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: Sending %d bytes of pagesize=%d",page->dataLen,page->size);
  //if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: called with proxy=%p SD=%d size=%d flags=%x",proxy,proxy->tls->socket,page->size,proxy->flags);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: called with flags=%x",proxy->flags);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: called with tls=%p",proxy->tls);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: called with SD=%d",proxy->tls->socket);
  if(proxy->flags&NB_PROXY_FLAG_WRITE_ERROR){
    nbLogMsg(context,0,'T',"nbProxySend: error flag is set");
    return(-1);
    }
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: calling nbProxyBookPutPage");
  nbProxyBookPutPage(context,&proxy->obook,page);
  proxy->flags&=0xff-NB_PROXY_FLAG_PRODUCER_STOP;
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: made it past bail out conditions");
  if(!(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT)){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyPutPage: nbListenerAddWrite SD=%d flags=%x",proxy->tls->socket,proxy->flags);
    nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyWriter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  return(0);
  }

/*
* Tell proxy we have written to the output book after calling nbProxyBookWriteWhere
* Here we schedule writing to peer.
*/
int nbProxyProduced(nbCELL context,nbProxy *proxy,int len){
  int rc;
  
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyProduced: called SD=%d len=%d",proxy->tls->socket,len);
  rc=nbProxyBookProduced(context,&proxy->obook,len);
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyProduced: nbProxyBookProduced rc=%d",rc);
  if(rc) return(rc);
  if(!(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT)){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyProduced: nbListenerAddWrite SD=%d flags=%x",proxy->tls->socket,proxy->flags);
    nbListenerAddWrite(context,proxy->tls->socket,proxy,nbProxyWriter);
    proxy->flags|=NB_PROXY_FLAG_WRITE_WAIT;
    }
  return(0);
  }

/*
*  Get page from proxy input book
*
*  Returns: pointer page or NULL if input book is empty
*/
nbProxyPage *nbProxyGetPage(nbCELL context,nbProxy *proxy){
  nbProxyPage *page;
  if(proxyTrace){
    if(proxy->tls) nbLogMsg(context,0,'I',"nbProxyGetPage: called for %s",nbTlsGetUri(proxy->tls));
    }
  page=nbProxyBookGetPage(context,&proxy->ibook);
  proxy->flags&=0xff-NB_PROXY_FLAG_CONSUMER_STOP;
  if(!(proxy->flags&NB_PROXY_FLAG_READ_WAIT) && proxy->tls){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyGetPage: nbListenerAdd SD=%d flags=%x",proxy->tls->socket,proxy->flags);
    nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyReader);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  // 2012-12-27 eat 0.8.13 - CID 751550 - included proxy->tls condition
  if(proxyTrace && page && proxy->tls) nbLogMsg(context,0,'T',"nbProxyGetPage: called with proxy=%p SD=%d size=%d flags=%x",proxy,proxy->tls->socket,page->size,proxy->flags);
  return(page);
  }

/*
* Tell proxy we have consumed data after calling nbProxyBookReadWhere
* Here we schedule reading from peer.
*/
int nbProxyConsumed(nbCELL context,nbProxy *proxy,int len){
  int rc;
  rc=nbProxyBookConsumed(context,&proxy->ibook,len);
  if(rc) return(rc);
  if(!(proxy->flags&NB_PROXY_FLAG_READ_WAIT) && proxy->tls){
    if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyConsumed: nbListenerAdd SD=%d flags=%x",proxy->tls->socket,proxy->flags);
    nbListenerAdd(context,proxy->tls->socket,proxy,nbProxyReader);
    proxy->flags|=NB_PROXY_FLAG_READ_WAIT;
    }
  return(0);
  }

/*
*  Shutdown proxy connection
*
*    Note: We may need to define a set of values for the code
*    parameter.  Currently we assume the caller to this function
*    and the proxy shutdown handler have a common understanding. 
*    It isn't clear that is always possible unless we provide
*    a definition to be used by all shutdown handlers and callers
*    of this function.
*/
int nbProxyShutdown(nbCELL context,nbProxy *proxy,int code){
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyShutdown: %s code=%d proxy=%p proxy->handle=%p",proxy->tls->uriMap[proxy->tls->uriIndex].uri,code,proxy,proxy->handle);
  if(proxy->shutdown) (*proxy->shutdown)(context,proxy,proxy->handle,code);
  if(proxy->flags&NB_PROXY_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,proxy->tls->socket);
    proxy->flags&=0xff-NB_PROXY_FLAG_WRITE_WAIT;
    }
  if(proxy->flags&NB_PROXY_FLAG_READ_WAIT){
    nbListenerRemove(context,proxy->tls->socket);
    proxy->flags&=0xff-NB_PROXY_FLAG_READ_WAIT;
    }
  if(proxy->tls){
    nbTlsClose(proxy->tls); // do this after the removes because it clears the socket
    nbTlsFree(proxy->tls);
    }
  proxy->flags=0;
  nbProxyBookClose(context,&proxy->ibook);
  nbProxyBookClose(context,&proxy->obook);
  nbFree(proxy,sizeof(nbProxy));
  return(0);
  }

int nbProxyDestroy(nbCELL context,nbProxy *proxy){
  if(proxyTrace) nbLogMsg(context,0,'T',"nbProxyDestroy: called");
  if(proxy->tls) nbLogMsg(context,0,'T',"nbProxyDestroy: uri=%s",proxy->tls->uriMap[0].uri);
  if(proxy->tls) nbTlsFree(proxy->tls);
  nbProxyBookClose(context,&proxy->ibook);
  nbProxyBookClose(context,&proxy->obook);
  nbFree(proxy,sizeof(nbProxy));
  return(0);
  }

#endif
