/*
* Copyright (C) 1998-2011 The Boeing Company
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
* File:     nbproxy.h
*
* Title:    Proxy API Header
*
* Function:
*
*   This header defines routines that implement the NodeBrain Proxy API. 
*
* See nbproxy.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010-01-07 eat 0.7.7  (original prototype) 
* 2012-05-11 eat 0.8.9  Included NB_PROXY_FLAG_PRODUCER_SHUTDOWN flag
* 2012-05-16 eat 0.8.9  Changed nbProxyConnect parameters - TLSX and URI replace tlsContext
* 2012-05-19 eat 0.8.9  Included NB_PROXY_FLAG_CLIENT to support failover
*=============================================================================
*/
#ifndef _NBPROXY_H_
#define _NBPROXY_H_       /* never again */

extern int proxyTrace;          // debugging trace flag for TLS routines

#define NB_PROXY_PAGESIZE 64*1024  // buffer length

typedef struct NB_PROXY_PAGE{
  struct NB_PROXY_PAGE *next;
  void         *data;            // data
  uint32_t      size;            // page size
  uint32_t      dataLen;         // data length
  unsigned char flags;           // see NB_PROXY_PAGE_FLAG_*
  } nbProxyPage;

#define NB_PROXY_PAGE_FLAG_CACHED 1  // static page - do not return to free pages

typedef struct NB_PROXY_BOOK{
  nbProxyPage   *writePage;
  nbProxyPage   *readPage;
  uint32_t       readOffset;
  } nbProxyBook;

typedef struct NB_PROXY{
  int              flags;   // see NB_PROXY_FLAG_* 
  struct NB_PROXY *other;   // Other proxy structure (proxy service mode) 
  nbTLS           *tls;     // TLS connection to client or server
  nbProxyBook      ibook;   // input book
  nbProxyBook      obook;   // output book
  void            *handle;
  int  (*producer)(nbCELL context,struct NB_PROXY *proxy,void *handle);
  int  (*consumer)(nbCELL context,struct NB_PROXY *proxy,void *handle);
  void (*shutdown)(nbCELL context,struct NB_PROXY *proxy,void *handle,int code);
  } nbProxy;

#define NB_PROXY_FLAG_WRITE_WAIT     1
#define NB_PROXY_FLAG_READ_WAIT      2
#define NB_PROXY_FLAG_WRITE_ERROR    4
#define NB_PROXY_FLAG_PRODUCER_STOP  8
#define NB_PROXY_FLAG_CONSUMER_STOP 16
#define NB_PROXY_FLAG_FINISH_OUTPUT 32 // shutdown when output book is empty
#define NB_PROXY_FLAG_FINISH_INPUT  64 // shutdown when input book is empty and socket is closed
#define NB_PROXY_FLAG_CLIENT       128 // client will failover retry while connecting
// Note: This can be extended above 128, but change 0xff-NB_PROXY_FLAG_xxx constructs to 0xffff-NB_PROXY_FLAG_xxxx

/*  A proxy service will use a proxy session with a pair of proxy structures
typedef struct NB_PROXY_SESSION{
  nbProxy *server;    // server proxy talks to a client
  void *serverPage;
  nbProxy *client;    // client proxy talks to a server
  void *clientPage;
  } nbProxySession;
*/

// API

extern nbProxyBook *nbProxyBookOpen(nbCELL context);
extern int nbProxyBookWriteWhere(nbCELL context,nbProxyBook *book,void **data);
extern int nbProxyBookProduced(nbCELL context,nbProxyBook *book,int len);
extern int nbProxyBookReadWhere(nbCELL context,nbProxyBook *book,void **data);
extern int nbProxyBookConsumed(nbCELL context,nbProxyBook *book,int len);
extern void *nbProxyBookClose(nbCELL context,nbProxyBook *book);
//extern int nbProxySendBook(nbCELL context,nbProxy *proxy,nbProxyBook *book);

extern nbProxyPage *nbProxyPageOpen(nbCELL context,void **data,int *size);
extern int nbProxyPageProduced(nbCELL context,nbProxyPage *page,int len);
extern void *nbProxyPageClose(nbCELL context,nbProxyPage *page);

extern nbProxy *nbProxyConstruct(nbCELL context,int client,nbCELL tlsContext,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));

extern void nbProxyProducer(nbCELL context,nbProxy *proxy,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle));

extern void nbProxyConsumer(nbCELL context,nbProxy *proxy,void *handle,
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle));

extern void nbProxyModify(nbCELL context,nbProxy *proxy,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));

extern int nbProxyListen(nbCELL context,nbProxy *proxy); 

extern int nbProxyConnectNext(nbCELL context,nbProxy *proxy);
extern nbProxy *nbProxyConnect(nbCELL context,nbTLSX *tlsx,char *uri,void *handle,
  int (*producer)(nbCELL context,nbProxy *proxy,void *handle),
  int (*consumer)(nbCELL context,nbProxy *proxy,void *handle),
  void (*shutdown)(nbCELL context,nbProxy *proxy,void *handle,int code));

extern nbProxyPage *nbProxyGetPage(nbCELL context,nbProxy *proxy);
extern int nbProxyPutPage(nbCELL context,nbProxy *proxy,nbProxyPage *page);
extern int nbProxyProduced(nbCELL context,nbProxy *proxy,int len);

extern void nbProxyForward(nbCELL context,nbProxy *client,nbProxy *server,int option);

extern int nbProxyConsumed(nbCELL context,nbProxy *proxy,int len);

extern int nbProxyShutdown(nbCELL context,nbProxy *proxy,int code);

extern int nbProxyDestroy(nbCELL context,nbProxy *proxy);

#endif
