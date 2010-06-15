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
*
* Program: NodeBrain
*
* File:    nb-tls.c
*
* Title:   Transport Layer Security
*
* Purpose:
*
*   This file provides functions for Transport Layer Security
*   using the OpenSSL library.  This API is designed to avoid
*   any dependence on a NodeBrain environment so it can be
*   easily used for other applications.
*
*  [We have started using the "nb-" naming convention to identify
*   files that are part of the NodeBrain API but independent of
*   the NodeBrain environment.]
*
* Description:
*
*   The OpenSSL library API provides everything we need for
*   encrypted and authenticated communcation.  Here we add
*   another layer to provide a simplified API, which also
*   allows for the possible addition of alternate security
*   layers with mininum impact on code using this API.
*
*   This API is designed to avoid any dependence on other portions
*   of the NodeBrain Library, so it can be used by programs that
*   do not establish a NodeBrain environment.
*   
*   The nbTlsCreateContext function is used to create a "context"
*   structure that describes a set of TLS parameters.  This is
*   used by clients and servers when establishing connections.
*
*     nbTLSX *nbTlsCreateContext(int option,void *handle,int timeout,char *keyFile,char *certFile,char *trustedCertsFile);
*
*   The nbTlsFreeContext function is used to free up a TLS context.
*
*     int nbTlsFreeContext(nbTLSX *tlsx);
*
*
*   The nbTlsConnect function are used by clients to establish
*   a connection to a server.
*
*     nbTLS *nbTlsConnect(nbTLSX *tlsx,char *host,int port); 
*    
*   The nbTlsListen function is used to get a listening socket.  This
*   function has nothing to do with TLS, and may be replaced with
*   nbTcpListen in the future when nbip.c is cleaned up.
*
*     int nbTlsListen(char *addr,unsigned short port);
*
*   The nbTlsAccept function is used by servers to establish a  
*   connection by accepting a connection request from a client.
*
*     nbTLS *nbTlsAccept(nbTLSX *tlsx,int socket,void *session);
*
*   The nbTlsRead function is used to read data from a peer.
*
*     int nbTlsRead(nbTLS *tls,char *buffer,size_t size,int timeout);
*
*   The nbTlsWrite functions is used to write data to a peer.
*
*     int nbTlsWrite(nbTLS *tls,char *buffer,size_t size,int timeout);
*
*   The nbTlsClose function is used to close a connection and free up
*   the TLS handle.
*
*     int nbTlsClose(nbTLS *tls);
*
*==================================================================================
* Change Summary
*
* Date       Name/Version/Change
* ---------- ----------------------------------------------------------------------
* 2009-07-11 Ed Trettevik - original prototype
* 2010-01-08 eat - modified to better support non-blocking sockets.
*            Previously nbTlsConnect and nbTlsAccept returned nbTLS
*            structures.  However, for non-blocking sockets we can
*            have multiple steps to a connection.  So new functions
*            nbTlsAlloc and nbTlsFree have been provided to
*            allocate and free nbTLS structures.  Functions like
*            nbTlsConnect and nbTlsAccept that previously returned
*            nbTLS structures now just operate on them.
* 2010-01-08 eat - converted to using a uri string for defining connections
*            We now allow for up to three separate uri values in a
*            string so failover values can be specified. 
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
*==================================================================================
*/
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#if !defined(WIN32)
#include <sys/un.h>
#endif

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <nb-tls.h>

/*
*  Look up the ipaddr of a host
*/
static char *nbTlsGetAddrByName(char *hostname){
  struct hostent *host;
  struct in_addr *inaddr;

  if((host=gethostbyname(hostname))==NULL) return(NULL);
  inaddr=(struct in_addr *)*(host->h_addr_list);
#if defined(mpe)
  return((char *)inet_ntoa(*inaddr));
#else
  return(inet_ntoa(*inaddr));
#endif
  }

/*
*  Parse a URI list and return an array of nbTlsUriMap structures
*
*    uriMap   - array of nbTlsUriMap structures
*    n        - number of entries in array
*    uriList  - string containing space separated uri list where each
*               entry if of the following form.
*
*                 scheme://name[:port]
*
*  Structure elements
*
*    uri      - a single uri
*    scheme   - integer representing the scheme portion
*                 file://   NB_TLS_SCHEME_FILE 
*                 unix://   NB_TLS_SCHEME_UNIX 
*                 tcp://    NB_TLS_SCHEME_TCP
*                 tls://    NB_TLS_SCHEME_TLS 
*                 https://  NB_TLS_SCHEME_HTTPS               
*    name     - the name portion (may be address)
*    address  - the addr portion (may be derived from name)
*    port     - port number 
*               
*  Returns
*
*    -2 - value too large for buffer
*    -1 - syntax error or value too large for buffer
*     n - number of entries parsed
*/
int nbTlsUriParse(nbTlsUriMap *uriMap,int n,char *uriList){
  char *delim,*uricur,*cursor;   
  int i;

  uricur=uriList;
  while(*uricur==' ') uricur++;
  for(i=0;i<n && *uricur;i++){
    *uriMap->uri=0;
    *uriMap->name=0;
    *uriMap->addr=0;
    uriMap->port=0;
    delim=strchr(uricur,' ');
    if(!delim) delim=strchr(uricur,0);
    if(delim-uricur>=sizeof(uriMap->uri)-1) return(-2);
    strncpy(uriMap->uri,uricur,delim-uricur);
    *(uriMap->uri+(delim-uricur))=0;
    uricur=delim;
    while(*uricur==' ') uricur++;
    cursor=uriMap->uri;
    if(strncmp(cursor,"file://",7)==0) uriMap->scheme=NB_TLS_SCHEME_FILE,cursor+=7;
    else if(strncmp(cursor,"unix://",7)==0) uriMap->scheme=NB_TLS_SCHEME_UNIX,cursor+=7;
    else if(strncmp(cursor,"tcp://",6)==0) uriMap->scheme=NB_TLS_SCHEME_TCP,cursor+=6;
    else if(strncmp(cursor,"tls://",6)==0) uriMap->scheme=NB_TLS_SCHEME_TLS,cursor+=6;
    else if(strncmp(cursor,"https://",8)==0) uriMap->scheme=NB_TLS_SCHEME_HTTPS,cursor+=8;
    else return(-1);
    if(uriMap->scheme==NB_TLS_SCHEME_FILE || uriMap->scheme==NB_TLS_SCHEME_UNIX) strcpy(uriMap->name,cursor); // size checked
    else{
      delim=strchr(cursor,':'); // port?
      if(delim){
        uriMap->port=atoi(delim+1);
        strncpy(uriMap->name,cursor,delim-cursor); // size checked
        *(uriMap->name+(delim-cursor))=0;
        }
      else strcpy(uriMap->name,cursor); // size checked
      cursor=uriMap->name;
      if(*cursor>='0' && *cursor<='9'){
        if(strlen(cursor)>sizeof(uriMap->addr)-1) return(-2);
        strcpy(uriMap->addr,cursor); // size checked
        }
      else{
        delim=nbTlsGetAddrByName(cursor); 
        if(!delim || strlen(delim)>sizeof(uriMap->addr)-1) return(-2);
        strcpy(uriMap->addr,delim); // size checked
        }
      }
    uriMap++; // step to next array entry
    }
  return(i);
  }

/*
* Get TLS context for server or client
*
*   Option: See NB_TLS_OPTION_* in nb-tls.h
*    
*/
nbTLSX *nbTlsCreateContext(int option,void *handle,int timeout,char *keyFile,char *certFile,char *trustedCertsFile){
  nbTLSX *tlsx;
  SSL_METHOD *method;
  SSL_CTX    *ctx=NULL;
  //char *anonymousCipher="ADH",*authCipher="AES",*cipher=authCipher;
  char *anonymousCipher="ADH",*authCipher="ALL",*cipher=authCipher;
  char *ctxContext="nbTls";
  static int initialize=1; // changed to 0 after we initialize

  if(option){  // not NB_TLS_OPTION_TCP
    if(initialize){
      SSL_load_error_strings();   // readable error messages
      SSL_library_init();         // initialize library
      // let's assume we have a system with /dev/urandom for now
      // otherwise we should include a call to RAND_seed()
      initialize=0;
      }
    // Set up the SSL context
    //if(option&NB_TLS_OPTION_CLIENT) method=TLSv1_client_method();
    //else method=TLSv1_server_method();
    if(option&NB_TLS_OPTION_CLIENT) method=SSLv2_client_method();
    else method=SSLv2_server_method();
    ctx=SSL_CTX_new(method);
    if(!ctx){
      fprintf(stderr,"nbTlsCreateContext: Unable to create SSL context using SSL_CTX_new()\n");
      return(NULL);
      }
    if(!SSL_CTX_set_session_id_context(ctx,(unsigned char *)ctxContext,strlen(ctxContext))){
      fprintf(stderr,"nbTlsCreateContext: Unable to SSL_CTX_set_session_id_context()\n");
      SSL_CTX_free(ctx);
      return(NULL);
      }
    if(!(option&NB_TLS_OPTION_CERT)) cipher=anonymousCipher;
    //if(!SSL_CTX_set_cipher_list(ctx,cipher)){
    //  fprintf(stderr,"nbTlsCreateContext: Unable to set cipher list.\n");
    //  SSL_CTX_free(ctx);
    //  return(NULL);
    //  }
    //if(option&NB_TLS_OPTION_CERT){
    if(option&NB_TLS_OPTION_TLS){
      if(!certFile || !*certFile || !keyFile || !*keyFile){
        fprintf(stderr,"nbTlsCreateContext: Certificate and key files required for option\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      if(option&NB_TLS_OPTION_CERTS) SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_CLIENT_ONCE,NULL);
      else SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE,NULL);
      //if(trustedCertsFile) fprintf(stderr,"trustedCertsFile=%s\n",trustedCertsFile);
      //else fprintf(stderr,"nbTlsCreateContext: no trustedCertsFile\n");
      if(trustedCertsFile && *trustedCertsFile && SSL_CTX_load_verify_locations(ctx,trustedCertsFile,NULL)<1){
        fprintf(stderr,"nbTlsCreateContext: Unable to load trusted certificates file.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      //if(certFile) fprintf(stderr,"certFile=%s\n",certFile);
      //else fprintf(stderr,"nbTlsCreateContext: no trustedCertsFile\n");
      if(certFile && *certFile && SSL_CTX_use_certificate_file(ctx,certFile,SSL_FILETYPE_PEM)<1){
        fprintf(stderr,"nbTlsCreateContext: Unable to load certificate file.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      //if(keyFile) fprintf(stderr,"keyFile=%s\n",keyFile);
      //else fprintf(stderr,"nbTlsCreateContext: no trustedCertsFile\n");
      if(keyFile && *keyFile && SSL_CTX_use_PrivateKey_file(ctx,keyFile,SSL_FILETYPE_PEM)<1){
        fprintf(stderr,"nbTlsCreateContext: Unable to load key file.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      if(((certFile && *certFile) || (keyFile && *keyFile)) && !SSL_CTX_check_private_key(ctx)){
        fprintf(stderr,"nbTlsCreateContext: Private key does not match the certificate public key.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      }
    else SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,NULL);
    // 2010-05-31 eat - commented out the following debugging line
    SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,NULL);
    }
  tlsx=malloc(sizeof(nbTLSX)); 
  memset(tlsx,0,sizeof(nbTLSX));
  tlsx->option=option;
  tlsx->timeout=timeout;
  tlsx->ctx=ctx;
  tlsx->handle=handle;
  return(tlsx);
  }


/*
*  Free a TLS context
*/
int nbTlsFreeContext(nbTLSX *tlsx){
  if(tlsx->ctx) SSL_CTX_free(tlsx->ctx);
  free(tlsx);
  return(0);
  }

//***************************************

/*
*  Construct TLS structure
*/
nbTLS *nbTlsCreate(nbTLSX *tlsx,char *uri){
  nbTLS *tls;
  int uriCount;

  tls=(nbTLS *)malloc(sizeof(nbTLS));
  memset(tls,0,sizeof(nbTLS));
  tls->tlsx=tlsx;
  if(tlsx) tls->handle=tlsx->handle;
  uriCount=nbTlsUriParse(tls->uriMap,3,uri);
  if(uriCount<1){
    free(tls);
    return(NULL);
    }
  tls->uriCount=uriCount;
  // NOTE: The tls->option of NB_TLS_OPTION_TLS should not be referenced in the future
  // Instead we need to look at tls->uriMap[tls->uriIndex].scheme
  if(tls->uriMap[0].scheme==NB_TLS_SCHEME_TLS || tls->uriMap[0].scheme==NB_TLS_SCHEME_HTTPS) tls->option|=NB_TLS_OPTION_TLS;
  return(tls);
  }

/*
*  Do some TLS stuff after a blocking or non-blocking connection
*/
int nbTlsConnected(nbTLS *tls){
  SSL *ssl=NULL;
  nbTLSX *tlsx=tls->tlsx;
  int rc,error;

  //fprintf(stderr,"nbTlsConnected: tlsx=%p tlsx->option=%d\n",tlsx,tlsx->option);
  if(tlsx && (tls->uriMap[tls->uriIndex].scheme==NB_TLS_SCHEME_TLS || tls->uriMap[tls->uriIndex].scheme==NB_TLS_SCHEME_HTTPS)){
    ssl=SSL_new(tlsx->ctx);
    if(!ssl){
      fprintf(stderr,"nbTlsConnected: SSL_new failed\n");
      close(tls->socket);
      return(-1);
      }
    SSL_set_fd(ssl,tls->socket);
    tls->ssl=ssl;

    rc=SSL_connect(tls->ssl);
    if(rc!=1){
      error=SSL_get_error(tls->ssl,rc); // get error code
      if(error==SSL_ERROR_WANT_WRITE || error==SSL_ERROR_WANT_READ) return(error);
      fprintf(stderr,"nbTlsConnected: Handshake failed - rc=%d code=%d\n",rc,error);
      ERR_print_errors_fp(stderr);
      SSL_shutdown(tls->ssl);
      close(tls->socket);
      SSL_free(tls->ssl);
      tls->ssl=NULL;
      return(error);
      }
    }
  if(tlsx){
    tls->option=tlsx->option;
    tls->handle=tlsx->handle;
    }
  else tls->option=NB_TLS_OPTION_TCP;
  return(0);
  }

/*
*  Connect non-blocking
*
*  Returns:  return code from connect()
*     -1 - error (see errno set by connect())
*      0 - connecting (errno==EINPROGRESS)
*      1 - connected
*/  
int nbTlsConnectNonBlocking(nbTLS *tls){
  int sd,rc;
  struct sockaddr_in sa;
  struct timeval tv;
  char *addr=tls->uriMap[tls->uriIndex].addr;
  int port=tls->uriMap[tls->uriIndex].port;

  fprintf(stderr,"nbTlsConnectNonBlocking: called addr=%s port=%d\n",addr,port);
  sd=socket(AF_INET,SOCK_STREAM,0);
  if(sd<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: Unable to obtain socket\n");
    return(-1);
    }
  memset(&sa,0,sizeof(sa));
  sa.sin_family=AF_INET;
  sa.sin_addr.s_addr=inet_addr(addr);   // host IP
  sa.sin_port=htons(port);              // port number

  if(tls->tlsx) tv.tv_sec=tls->tlsx->timeout;
  else tv.tv_sec=5;
  tv.tv_usec=0;
  rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
  if(rc<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
    return(-1);
    }
  rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
  if(rc<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
    return(-1);
    }
  // 2010-06-06 eat - seeing if blocking IO will work
  //fcntl(sd,F_SETFL,fcntl(sd,F_GETFL)|O_NONBLOCK);
  rc=connect(sd,(struct sockaddr*)&sa,sizeof(sa));
  if(rc<0){
    if(errno==EINPROGRESS){
      tls->socket=sd;
      fprintf(stderr,"nbTlsConnectNonBlocking: connecting\n");
      return(0);
      }
    fprintf(stderr,"nbTlsConnectNonBlocking: connect failed: %s\n",strerror(errno));
    close(sd);
    return(-1);
    }
  tls->socket=sd;
  return(1);
  }

/*
*  Connect to one of multiple servers from a client
*
*    This function attempts a connect to the first
*    uri specified when the tls structure was created
*    If it is unable to get a connection, and more
*    servers were specified, it continues throught
*    the list.
*
*    It supports unix domain sockets as well as
*    internet domain sockets.
*/
int nbTlsConnect(nbTLS *tls){
  int sd,rc;
  struct sockaddr_in sa;
  struct timeval tv;
  SSL *ssl=NULL;
  int uriIndex;
  struct sockaddr_un un_addr;

  for(uriIndex=0;uriIndex<tls->uriCount;uriIndex++){
    if(tls->uriMap[uriIndex].scheme==NB_TLS_SCHEME_UNIX){
      if(strlen(tls->uriMap[uriIndex].name)>sizeof(un_addr.sun_path)){
        fprintf(stderr,"nbTlsListen: Local domain socket path too long - %s\n",tls->uriMap[uriIndex].name);
        continue;
        }
      sd=socket(AF_UNIX,SOCK_STREAM,0);
      if(sd<0){
        fprintf(stderr,"nbTlsConnect: Unable to obtain socket\n");
        return(-1);
        }
      un_addr.sun_family = AF_UNIX;
      strcpy(un_addr.sun_path,tls->uriMap[uriIndex].name);
      rc=connect(sd,(struct sockaddr *)&un_addr,sizeof(un_addr));
      }
    else{ // internet domain
      sd=socket(AF_INET,SOCK_STREAM,0);
      if(sd<0){
        fprintf(stderr,"nbTlsConnect: Unable to obtain socket\n");
        return(-1);
        }
      memset(&sa,0,sizeof(sa));
      sa.sin_family=AF_INET;
      sa.sin_addr.s_addr=inet_addr(tls->uriMap[uriIndex].addr);   // host IP 
      sa.sin_port=htons(tls->uriMap[uriIndex].port);              // port number
    
      if(tls->tlsx) tv.tv_sec=tls->tlsx->timeout;
      else tv.tv_sec=5;
      tv.tv_usec=0;
      //rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
      //if(rc==-1){
      //  fprintf(stderr,"nbTlsConnect: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
      //  return(-1);
      //  } 
      //rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
      //if(rc==-1){
      //  fprintf(stderr,"nbTlsConnect: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
      //  return(-1);
      //  } 
      rc=connect(sd,(struct sockaddr*)&sa,sizeof(sa));
      }
    if(rc<0) close(sd);
    else{
      tls->uriIndex=uriIndex;
      tls->socket=sd;
      tls->ssl=ssl;
      tls->handle=tls->tlsx->handle;
      return(nbTlsConnected(tls));
      }
    }
  fprintf(stderr,"nbTlsConnect: connect failed: %s\n",strerror(errno));
  return(-1);
  }

/*
*  SSL handshake with nonblocking errors allowed
*/
int nbTlsHandshakeNonBlocking(nbTLS *tls){
  int rc,error;
  nbTLSX *tlsx=tls->tlsx;
  SSL *ssl=NULL;

  fprintf(stderr,"nbTlsHandshakeNonBlocking: tls->option=%d tls->tlsx=%p tls->ssl=%p\n",tls->option,tls->tlsx,tls->ssl);
  if(!tls->option) return(0);
  if(!tlsx){
    fprintf(stderr,"nbTlsHandshakeNonBlocking: Logic error - should not be called will null tlsx - terminating\n");
    exit(1);
    }
  fprintf(stderr,"nbTlsHandshakeNonBlocking: tls->tlsx->ctx=%p\n",tlsx->ctx);
  ssl=SSL_new(tlsx->ctx);
  if(!ssl){
    fprintf(stderr,"nbTlsHandshakeNonBLocking: SSL_new failed\n");
    return(-1);
    }
  SSL_set_fd(ssl,tls->socket);
  tls->ssl=ssl;
  fprintf(stderr,"nbTlsHandshakeNonBlocking: tls->socket=%u ssl=%p\n",tls->socket,ssl);
  rc=SSL_connect(tls->ssl);
  if(rc!=1){
    error=SSL_get_error(tls->ssl,rc); // get error code
    if(error==SSL_ERROR_WANT_WRITE || error==SSL_ERROR_WANT_READ) return(error);
    fprintf(stderr,"nbTlsHandshakeNonBlocking: Handshake failed - rc=%d code=%d\n",rc,error);
    ERR_print_errors_fp(stderr);
    SSL_shutdown(tls->ssl);
    close(tls->socket);
    SSL_free(tls->ssl);
    tls->ssl=NULL;
    return(error);
    }
  return(0);
  }

/*
*  Get a socket for listening
*
*    We provide this function for independence until the
*    nbip.c API is cleaned up.  This can replace nbIpListen
*    eventually.  When the nbip.c API is no longer dependent
*    on a NodeBrain environment (e.g. no calls to outMsg), 
*    then it can be used as a compliment to the nb-tls.c API for
*    operations on sockets that have nothing to do with TLS.
*
*    If addr contains a '/' character, a unix domain socket
*    is create using the specified path and the port is ignored
*/
int nbTlsListen(nbTLS *tls){
  int sd,sockopt_enable=1;
  struct sockaddr_in in_addr;
  int domain=AF_INET;
#if !defined(WIN32)
  struct sockaddr_un un_addr;
  char *addr=tls->uriMap[tls->uriIndex].addr;
  char *name=tls->uriMap[tls->uriIndex].name;
  unsigned short port=tls->uriMap[tls->uriIndex].port;

  if(tls->uriMap[tls->uriIndex].scheme==NB_TLS_SCHEME_UNIX){
    domain=AF_UNIX;
    if(strlen(name)>sizeof(un_addr.sun_path)){
      fprintf(stderr,"nbTlsListen: Local domain socket path too long - %s\n",name);
      return(-1);
      }
    }
#endif
#if defined(WIN32)
  // HANDLE dup,hProcess = GetCurrentProcess();
  if((sd=socket(domain,SOCK_STREAM,0))==INVALID_SOCKET) {
    fprintf(stderr,"nbTlsListen: Unable to create socket. errno=%d\n",WSAGetLastError());
    return(sd);
    }
  // Prevent inheritance by child processes
  if(!SetHandleInformation((HANDLE)sd,HANDLE_FLAG_INHERIT,0)){
    fprintf(stderr,"nbTlsListen: Unable to turn off socket inherit flag");
    closesocket(sd);
    return(INVALID_SOCKET);
    }
#else
  if ((sd=socket(domain,SOCK_STREAM,0)) < 0) {
    fprintf(stderr,"nbTlsListen: Unable to create socket - %s\n",strerror(errno));
    return(sd);
    }
  fcntl(sd,F_SETFD,FD_CLOEXEC);
#endif
  /* make sure we can reuse sockets when we restart */
  /* we say the option value is char for windows */
  if(setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,(char *)&sockopt_enable,sizeof(sockopt_enable))<0){
    fprintf(stderr,"nbTlsListen: Unable to set socket option - %s\n",strerror(errno));
#if defined(WIN32)
    closesocket(sd);
#else
    close(sd);
#endif
    return(-1);
    }
  if(domain==AF_INET){
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
    if(*addr==0) in_addr.sin_addr.s_addr=INADDR_ANY;
    else in_addr.sin_addr.s_addr=inet_addr(addr);
    if(bind(sd,(struct sockaddr *)&in_addr,sizeof(in_addr)) < 0) {
      fprintf(stderr,"nbTlsListen: Unable to bind to inet domain socket %d - %s\n",port,strerror(errno));
#if defined(WIN32)
      closesocket(sd);
#else
      close(sd);
#endif
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain sockets
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,name);
    unlink(name);
    if(bind(sd,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      fprintf(stderr,"nbTlsListen: Unable to bind to local domain socket %s - %s\n",name,strerror(errno));
#if defined(WIN32)
      closesocket(sd);
#else
      close(sd);
#endif
      return(-1);
      }
    }
#endif
  if(listen(sd,10)!=0) {
    fprintf(stderr,"nbTlsListen: Unable to listen - %s",strerror(errno));
#if defined(WIN32)
    closesocket(sd);
#else
    close(sd);
#endif
    return(-2);
    }
  tls->socket=sd;
  return(sd);
  }

/*
*  Accept a connection from a client
*
*    The tlsx parameter may be NULL, in which case the connection will
*    not be secured by TLS encryption or authentication.  This is as
*    if tlsx were not NULL, but the option specified not to use TLS.
*
*    The timeout parameter is used to set the timeout period for read
*    and write operations.  It does not specify a timeout on inactivity.
*    That must be implemented at a high level.
*
*    
*/
nbTLS *nbTlsAccept(nbTLS *tlsListener){
  nbTLS *tls;
  struct timeval tv;
  int sd,rc;
  char *protocol;
  struct sockaddr_in client;
#if defined(WIN32) || defined(mpe) || defined(SOLARIS) || defined(MACOS)
  size_t sockaddrlen;
#else
  socklen_t sockaddrlen;
#endif
  int socket=tlsListener->socket;
  sockaddrlen=sizeof(client);

#if defined(MACOS)
  sd=accept(socket,(struct sockaddr *)&client,(int *)&sockaddrlen);
#else
  sd=accept(socket,(struct sockaddr *)&client,&sockaddrlen);
#endif
  if(sd<0){
    if(errno!=EINTR) fprintf(stderr,"nbTlsAccept: accept failed - %s",strerror(errno));
    return(NULL);
    }
#if !defined(WIN32)
  fcntl(sd,F_SETFD,FD_CLOEXEC);
#endif
  if(tlsListener->tlsx) tv.tv_sec=tlsListener->tlsx->timeout;
  else tv.tv_sec=5;
  tv.tv_usec=0;
  rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
  if(rc==-1){
    fprintf(stderr,"nbTlsAccept: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
    return(NULL);
    }
  rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
  if(rc==-1){
    fprintf(stderr,"nbTlsAccept: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
    return(NULL);
    }
  // Create the nbTLS structure
  tls=malloc(sizeof(nbTLS));
  memset(tls,0,sizeof(nbTLS));
  if(tlsListener->tlsx) tls->option=tlsListener->tlsx->option;
  else tls->option=NB_TLS_OPTION_TCP;
  tls->socket=sd;
  tls->uriCount=1;
  tls->tlsx=tlsListener->tlsx;
#if defined(mpe)
  strcpy(tls->uriMap[0].addr,(char *)inet_ntoa(client.sin_addr));
#else
  strcpy(tls->uriMap[0].addr,inet_ntoa(client.sin_addr));
#endif
  tls->uriMap[0].port=ntohs(client.sin_port);
  fprintf(stderr,"nbTlsAccept: tls->option=%d\n",tls->option);
  if(tls->tlsx && tls->option&NB_TLS_OPTION_TLS){
    protocol="tls";
    tls->ssl=SSL_new(tls->tlsx->ctx);
    if(!tls->ssl){
      fprintf(stderr,"nbTlsAccept: SSL_new failed.\n");
      nbTlsFree(tls);
      return(NULL);
      }
    SSL_set_fd(tls->ssl,sd);
    //SSL_set_ex_data(ssl,0,session);  // make session structure available to verify callback function

    fprintf(stderr,"nbTlsAccept: Issuing SSL_accept on socket %d\n",sd);
    if((rc=SSL_accept(tls->ssl))!=1){
      char errbuf[121];
      SSL_load_error_strings();
      fprintf(stderr,"nbTlsAccept: SSL_accept rc=%d code=%d\n",rc,SSL_get_error(tls->ssl,rc));
      fprintf(stderr,"nbTlsAccept: %s\n",ERR_error_string(SSL_get_error(tls->ssl,rc),errbuf));
      ERR_print_errors_fp(stderr);
      nbTlsFree(tls);
      return(NULL);
      }
    fprintf(stderr,"nbTlsAccept: SSL connection using %s\n",SSL_get_cipher(tls->ssl));
    }
  else{
    fprintf(stderr,"nbTlsAccept: using clear tcp instead of tls\n");
    protocol="tcp";
    tls->ssl=NULL;
    }
  sprintf(tls->uriMap[0].uri,"%s://%s:%d",protocol,tls->uriMap[0].addr,tls->uriMap[0].port);
  if(tls->tlsx) tls->handle=tls->tlsx->handle;
  return(tls);
  }

/*
*  Read from peer
*/
int nbTlsRead(nbTLS *tls,char *buffer,size_t size){
  int len;
  fprintf(stderr,"nbTlsRead: size=%d\n",(int)size);
  if(!tls->ssl){
    fprintf(stderr,"nbTlsRead: calling clear recv\n");
    len=recv(tls->socket,buffer,size,0);
    while(len==-1 && errno==EINTR) len=recv(tls->socket,buffer,size,0);
    fprintf(stderr,"nbTlsRead: read len=%d\n",len);
    return(len);
    }
  fprintf(stderr,"nbTlsRead: calling SSL_read\n");
  len=SSL_read(tls->ssl,buffer,size);
  if(len<0){
    fprintf(stderr,"nbTlsRead: SSL_read rc=%d code=%d\n",len,SSL_get_error(tls->ssl,len));
    ERR_print_errors_fp(stderr);
    }
  fprintf(stderr,"nbTlsRead: SSL_read len=%d\n",len);
  return(len);
  }

/*
*  Write to peer
*/
int nbTlsWrite(nbTLS *tls,char *buffer,size_t size){
  int len;
  //fprintf(stderr,"nbTlsWrite: size=%d\n",(int)size);
  if(!tls->ssl){
    //fprintf(stderr,"nbTlsWrite: calling clear send - socket=%d\n",tls->socket);
    len=send(tls->socket,buffer,size,0);
    while(len==-1 && errno==EINTR) len=send(tls->socket,buffer,size,0);
    //fprintf(stderr,"nbTlsWrite: wrote len=%d\n",len);
    return(len);
    }
  //fprintf(stderr,"nbTlsWrite: calling SSL_write - ssl=%p\n",tls->ssl);
  len=SSL_write(tls->ssl,buffer,size);
  if(len<0){
    fprintf(stderr,"nbTlsWrite: SSL_write rc=%d code=%d\n",len,SSL_get_error(tls->ssl,len));
    ERR_print_errors_fp(stderr);
    }
  fprintf(stderr,"nbTlsWrite: SSL_write len=%d\n",len);
  return(len);
  }

/*
*  Close a TLS connection and free the structure
*/
int nbTlsClose(nbTLS *tls){
  int rc;

  if(!tls) return(0);
  if(tls->ssl){
    SSL_shutdown(tls->ssl);
    SSL_free(tls->ssl);
    tls->ssl=NULL;
    }
  if(tls->socket){
#if defined(WIN32)
    rc=closesocket(tls->socket);
#else
    rc=close(tls->socket);
#endif
    tls->socket=0;
    }
  return(rc);
  }

/*
*  Free TLS structure
*
*    We don't free the nbTLSX structure because it may be shared.
*    It is the applications responsibility to call nbTlsFreeContext
*    when appropriate.
*/
int nbTlsFree(nbTLS *tls){
  if(tls->socket || tls->ssl) nbTlsClose(tls);
  free(tls);
  return(0);
  }
