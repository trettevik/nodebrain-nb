/*
* Copyright (C) 1998-2014 The Boeing Company
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
* File:    nbtls.c
*
* Title:   Transport Layer Security
*
* Purpose:
*
*   This file provides functions for Transport Layer Security
*   using the OpenSSL library.  This API is designed with two
*   sets of functions.  The first set may be used without first
*   establishing a NodeBrain context.  The second set require
*   a NodeBrain context and enable configuration using NodeBrain
*   rules.
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
*   The nbTlsConnect function is used by clients to establish
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
*   ==============================================================================
*
*   We support a couple different ways of configuring a TLS
*   context.  For node modules it is most convenient to use
*   NodeBrain terms to specify configuration options.
*
*     define mytls node;
*     mytls. define Transport cell "any"; # "TLSv1" "SSLv3" "SSLv2"
*     mytls. define Authenticate cell "yes"; # "no"
*     mytls. define trustfile cell "security/TrustedCertificates.pem";
*     mytls. define certfile cell "security/ServerCertificates.pem";
*     mytls. define keyfile cell "security/ServerKey.pem";
*
*   These terms can be defined within a server node, because they are
*   not referenced until the node is enabled, generally when the agent
*   daemonizes.  However, for client nodes, a configuration node must be
*   specified prior to the specification of the client node, or the client
*   must load the configuration script.  Because loading a configuration
*   script seems like a generally useful feature, we have extend the
*   NodeBrain API to provide this service to any NodeBrain module.
*
*     int nbTermSource(nbCELL context,char *filename);
*
*   We should consider extending the syntax for nodes to include a configuration
*   file that is made available to the constructor, and a source file that is
*   automatically sourced on return from the constructor.
*
*     define myterm node[{config}][(source)] <skill>[(<args>)[:<specification>]][;]
*
*   For now, we will leave it up to the node module to determine how the config
*   file should be identified.
*
*   Another approach is for a client node to delay enabling until first referenced
*   or explicitely enabled.
*
*   In any case, here we are just assuming that a context is available for looking
*   up configuration parameters when we are called to initialize a TLS context.
*
*   If file is NULL or the empty string, the context is searched as is.  Otherwise
*   the file is first sourced into the context.
*
*     nbTLSX nbTlsCreateContext(nbCELL context,char *file);
*
*==================================================================================
* Change Summary
*
* Date       Name/Version/Change
* ---------- ----------------------------------------------------------------------
* 2009-06-01 Ed Trettevik (version 0.7.7)
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
* 2010-06-06 eat 0.8.2  included client parameter to nbTlsLoadContext
* 2010-06-16 eat 0.8.2  Included timeout in connect function
* 2010-06-16 eat 0.8.2  Put debug messages under tlsTrace option
* 2010-10-21 eat 0.8.5  Included nbTlsReconnectIfBetter function
*            This function does nothing if the uriIndex is zero, meaning we
*            are already connected to the first URI.  If we are connected to
*            URI N, where N>0, then this function will attempt to connect to
*            a URI less than N.  If not successful the function returns zero
*            and the current connection is retained.  If a connection to a
*            URI less than N is obtained, the the current socket is closed and
*            the new connection is used.  A program may call this function
*            periodically to re-establish a preferred connection. 
* 2011-02-08 eat 0.8.5  Minor cleanup for local domain sockets
* 2011-10-26 eat 0.8.6  Stopped requesting certificate if option is "tls" only
* 2012-02-05 eat 0.8.7  Set unix socket file permission to 660
* 2012-04-14 eat 0.8.7  Disabled SSLv2 and or better security
* 2012-04-14 eat 0.8.7  Tried to clean up nbTlsCreate a bit
*            Previously, an nbTLS client connecting to a SSL/TLS server that did not
*            request client certificates was still required to provide certificate
*            and key files. This defect has been fixed.  Also, clients and servers
*            requesting an encrypted but unauthenticated connection were required
*            to have a full set of certificates.  This defect has been fixed with
*            little benefit since authenticated connections are prefered.
* 2012-04-21 eat 0.8.7  Included NB_TLS_ERROR_HANDSHAKE in nbTlsAccept
* 2012-04-21 eat 0.8.7  Included NB_TLS_OPTION_SSL2 option for compatibility with old appplications
* 2012-05-23 eat 0.8.9  Increased nbTlsCreate max number of URI's to 4
* 2012-12-27 eat 0.8.13 Checker updates
* 2012-12-30 eat 0.8.13 Modified nbLoadListener to only load context for secure protocols
* 2013-01-01 eat 0.8.13 Checker updates
* 2014-01-12 eat 0.9.00 added const to SSL_METHOD pointer
* 2014-01-25 eat 0.9.00 Checker updates
*==================================================================================
*/
#include <nb/nb.h>

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <nb/nbtls.h>

int tlsTrace;          // debugging trace flag for TLS routines

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
*  Return the URI of an open nbTLS structure
*/
char *nbTlsGetUri(nbTLS *tls){
  return(tls->uriMap[tls->uriIndex].uri);
  }

/*
*  Parse a URI list and return an array of nbTlsUriMap structures
*
*    uriMap   - array of nbTlsUriMap structures
*    n        - number of entries in array
*    uriList  - string containing space or comma separated uri list where each
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
  size_t size;

  uricur=uriList;
  while(*uricur==' ') uricur++;
  for(i=0;i<n && *uricur;i++){
    *uriMap->uri=0;
    *uriMap->name=0;
    *uriMap->addr=0;
    uriMap->port=0;
    delim=strchr(uricur,' ');
    if(!delim) delim=strchr(uricur,',');
    if(!delim) delim=uricur+strlen(uricur);  
    size=delim-uricur;
    if(size>=sizeof(uriMap->uri)-1) return(-2);  // 2013-01-01 eat - VID 5138-0.8.13-1 FP size checked
    if(size>=sizeof(uriMap->name)-1) return(-2);  // 2013-01-01 eat - uri and name are the same size, but help the checker by checking both
    strncpy(uriMap->uri,uricur,size);
    if(*delim==',') delim++;
    uricur=delim;
    while(*uricur==' ') uricur++;
    cursor=uriMap->uri;
    if(strncmp(cursor,"file://",7)==0) uriMap->scheme=NB_TLS_SCHEME_FILE,cursor+=7;
    else if(strncmp(cursor,"unix://",7)==0) uriMap->scheme=NB_TLS_SCHEME_UNIX,cursor+=7;
    else if(strncmp(cursor,"tcp://",6)==0) uriMap->scheme=NB_TLS_SCHEME_TCP,cursor+=6;
    else if(strncmp(cursor,"http://",7)==0) uriMap->scheme=NB_TLS_SCHEME_TCP,cursor+=7;
    else if(strncmp(cursor,"tls://",6)==0) uriMap->scheme=NB_TLS_SCHEME_TLS,cursor+=6;
    else if(strncmp(cursor,"https://",8)==0) uriMap->scheme=NB_TLS_SCHEME_HTTPS,cursor+=8;
    else return(-1);
    if(uriMap->scheme==NB_TLS_SCHEME_FILE || uriMap->scheme==NB_TLS_SCHEME_UNIX) strcpy(uriMap->name,cursor); // size checked // 2013-01-01 eat - VID 5378-0.8.13-1 FP
    else{
      delim=strchr(cursor,':'); // port?
      if(delim){
        uriMap->port=atoi(delim+1);
        strncpy(uriMap->name,cursor,size); // size checked // 2013-01-01 eat - VID 5447-0.8.13-1 FP
        *(uriMap->name+(delim-cursor))=0;
        }
      else strcpy(uriMap->name,cursor); // size checked // 2013-01-01 eat - VID 5326-0.8.13-1 FP
      cursor=uriMap->name;
      if(*cursor>='0' && *cursor<='9'){
        if(strlen(cursor)>sizeof(uriMap->addr)-1) return(-2);
        strcpy(uriMap->addr,cursor); // size checked  // 2013-01-01 eat - VID 5128-0.8.13-1 FP
        }
      else{
        delim=nbTlsGetAddrByName(cursor); 
        if(!delim || strlen(delim)>sizeof(uriMap->addr)-1) return(-2);
        strcpy(uriMap->addr,delim); // size checked   // 2013-01-01 eat - VID 4944-0.8.13-1 FP
        }
      }
    uriMap++; // step to next array entry
    }
  return(i);
  }

/*
*  Phony verification routine
*    This is experimental - user function can be added a parameter to nbTlsCreateContext
*/
static int nbTlsVerify(int preverify_ok,X509_STORE_CTX *ctx){
  return(1);
  }

/*
* Get TLS context for server or client
*
*   Option: See NB_TLS_OPTION_* in nb-tls.h
*    
*/
nbTLSX *nbTlsCreateContext(int option,void *handle,int timeout,char *keyFile,char *certFile,char *trustedCertsFile){
  nbTLSX *tlsx;
  const SSL_METHOD *method;
  SSL_CTX    *ctx=NULL;
  char *ctxContext="nbTls";
  static int initialize=1; // changed to 0 after we initialize

  if(!(option&NB_TLS_OPTION_CLIENT)) option|=NB_TLS_OPTION_SERVER;  // stay compatible with code that didn't set the server bit
  if(option&NB_TLS_OPTION_TLS){  // Using TLS/SSL
    fprintf(stderr,"nbTlsCreateContext: option=%d\n",option);
    if(initialize){
      SSL_load_error_strings();   // readable error messages
      SSL_library_init();         // initialize library
      // let's assume we have a system with /dev/urandom for now
      // otherwise we should include a call to RAND_seed()
      initialize=0;
      }
    // Set up the SSL context - SSLv23-SSLV2= SSLv3 or TLS1
    if(option&NB_TLS_OPTION_CLIENT) method=SSLv23_client_method();
    else method=SSLv23_server_method();
    ctx=SSL_CTX_new(method);
    if(!ctx){
      fprintf(stderr,"nbTlsCreateContext: Unable to create SSL context using SSL_CTX_new()\n");
      return(NULL);
      }
    if(!(option&NB_TLS_OPTION_SSL2)){                     // if not in old code compatibility mode
      SSL_CTX_set_options(ctx,SSL_OP_NO_SSLv2);           // disable SSLv2 for better security
      if(!SSL_CTX_set_cipher_list(ctx,"HIGH:MEDIUM")){    // and drop weak ciphers
        fprintf(stderr,"nbTlsCreateContext: Unable to set cipher list.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      }
    if(!SSL_CTX_set_session_id_context(ctx,(unsigned char *)ctxContext,strlen(ctxContext))){
      fprintf(stderr,"nbTlsCreateContext: Unable to SSL_CTX_set_session_id_context()\n");
      SSL_CTX_free(ctx);
      return(NULL);
      }
    // 2012-04-14 eat 0.8.7 - changed back to checking CERT option here
    if(option&NB_TLS_OPTION_CERT || option&NB_TLS_OPTION_SSL2){   // only fuss certificates if using CERT option - or compatibility
      fprintf(stderr,"nbTlsCreateContext: checking for certificate files\n");
      if(option&NB_TLS_OPTION_CERTS || option&NB_TLS_OPTION_CLIENT){   // authenticate peer to self
        if(option&NB_TLS_OPTION_CERTO)  // NOTE: the SSL_VERIFY_FAIL_IF_NO_PEER_CERT and SSL_VERIFY_CLIENT_ONCE have no impact on clients
          SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE,nbTlsVerify);
          //SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,nbTlsVerify);
        else SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_CLIENT_ONCE,NULL);
        fprintf(stderr,"nbTlsCreateContext: checking for trusted certificates file\n");
        if(!trustedCertsFile || !*trustedCertsFile){
          fprintf(stderr,"nbTlsCreateContext: Trusted certificates file required for specified option.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        if(SSL_CTX_load_verify_locations(ctx,trustedCertsFile,NULL)<1){
          fprintf(stderr,"nbTlsCreateContext: Unable to load trusted certificates file.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        }
      if(option&NB_TLS_OPTION_CERTS || option&NB_TLS_OPTION_SERVER){ // authenticate self to peer
        fprintf(stderr,"nbTlsCreateContext: checking for certificate and key files\n");
        if(!certFile || !*certFile || !keyFile || !*keyFile){
          fprintf(stderr,"nbTlsCreateContext: Certificate and key files required for option\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        if(SSL_CTX_use_certificate_file(ctx,certFile,SSL_FILETYPE_PEM)<1){
          fprintf(stderr,"nbTlsCreateContext: Unable to load certificate file.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        if(SSL_CTX_use_PrivateKey_file(ctx,keyFile,SSL_FILETYPE_PEM)<1){
          fprintf(stderr,"nbTlsCreateContext: Unable to load key file.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        if(!SSL_CTX_check_private_key(ctx)){
          fprintf(stderr,"nbTlsCreateContext: Private key does not match the certificate public key.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        }
      fprintf(stderr,"nbTlsCreateContext: certificates loaded\n");
      }
    else{
      fprintf(stderr,"nbTlsCreateContext: setting anonymous cipher\n");
      if(!SSL_CTX_set_cipher_list(ctx,"ADH-AES256-SHA")){
        fprintf(stderr,"nbTlsCreateContext: Unable to set cipher list.\n");
        SSL_CTX_free(ctx);
        return(NULL);
        }
      // set DH parameters if server
      if(option&NB_TLS_OPTION_SERVER && keyFile && keyFile){
        FILE *dhfile=fopen(keyFile,"r"); // 2013-01-01 eat - VID 5450-0.8.13-1 Intentional use of name from control file 
        DH *dh;
        if(dhfile){
          dh=PEM_read_DHparams(dhfile, NULL, NULL, NULL);
          fclose(dhfile);
          }
        else{
          fprintf(stderr,"nbTlsCreateContext: Unable to load DH parameters from keyfile.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        if(SSL_CTX_set_tmp_dh(ctx,dh)<0){
          fprintf(stderr,"nbTlsCreateContext: Unable to set DH parameters.\n");
          SSL_CTX_free(ctx);
          return(NULL);
          }
        }
      }
    }
  // 2012-10-13 eat - replaced malloc
  tlsx=nbAlloc(sizeof(nbTLSX)); 
  memset(tlsx,0,sizeof(nbTLSX));
  tlsx->option=option;
  tlsx->timeout=timeout;
  tlsx->ctx=ctx;
  // 2012-05-22 eat - ending the experimenting - had code depending on it
  // 2012-05-15 eat - experimenting without handle on tlsx
  tlsx->handle=handle;
  return(tlsx);
  }


/*
*  Free a TLS context
*/
int nbTlsFreeContext(nbTLSX *tlsx){
  if(tlsx->ctx) SSL_CTX_free(tlsx->ctx);
  // 2012-10-13 eat - replaced free
  nbFree(tlsx,sizeof(nbTLSX));
  return(0);
  }

//***************************************

/*
*  Construct TLS structure
*/
nbTLS *nbTlsCreate(nbTLSX *tlsx,char *uri){
  nbTLS *tls;
  int uriCount;

  // 2012-10-13 eat - replaced malloc
  tls=(nbTLS *)nbAlloc(sizeof(nbTLS));
  memset(tls,0,sizeof(nbTLS));
  tls->tlsx=tlsx;
  // 2012-05-22 eat - ending experimenting  - had code depending on it
  // 2012-05-15 eat - experimenting without handle on tlsx
  if(tlsx) tls->handle=tlsx->handle;
  uriCount=nbTlsUriParse(tls->uriMap,4,uri);
  if(uriCount<1){
    // 2012-10-13 eat - replaced free
    nbFree(tls,sizeof(nbTLS));
    return(NULL);
    }
  tls->uriCount=(unsigned char)(uriCount%256);  // 2013-01-01 eat - VID 4607-0.8.13-1 FP Can't be greater than 4 passed into nbTlsUriParse
  // NOTE: The tls->option of NB_TLS_OPTION_TLS should not be referenced in the future
  // Instead we need to look at tls->uriMap[tls->uriIndex].scheme
  if(tls->uriMap[0].scheme==NB_TLS_SCHEME_TLS || tls->uriMap[0].scheme==NB_TLS_SCHEME_HTTPS) tls->option|=NB_TLS_OPTION_TLS;
  return(tls);
  }

/*
*  Do some TLS stuff after a blocking or non-blocking connection
*
*  Return Code:
*
*    -1 - error socket closed
*     0 - connected or connecting without SSL
*     1 - SSL_ERROR_WANT_WRITE
*     2 - SSL_ERROR_WANT_READ
*/
int nbTlsConnected(nbTLS *tls){
  SSL *ssl=NULL;
  nbTLSX *tlsx=tls->tlsx;
  int rc,error;

  if(tlsTrace) fprintf(stderr,"nbTlsConnected: tlsx=%p tlsx->option=%d\n",tlsx,tlsx->option);
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
      if(error==SSL_ERROR_WANT_WRITE) return(1);
      if(error==SSL_ERROR_WANT_READ) return(2);
      fprintf(stderr,"nbTlsConnected: Handshake failed - rc=%d code=%d\n",rc,error);
      ERR_print_errors_fp(stderr);
      SSL_shutdown(tls->ssl);
      close(tls->socket);
      SSL_free(tls->ssl);
      tls->ssl=NULL;
      return(-1);
      }
    }
  if(tlsx){
    tls->option=tlsx->option;
    // 2012-05-22 eat - ended the experiment - had code depending on this
    // 2012-05-15 eat - experimenting without handle on tlsx
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
*      0 - connected
*      1 - connecting (errno==EINPROGRESS)  SSL_write wait
*      2 - connecting (errno==EINPROGRESS)  SSL_read wait
*      3 - connecting (errno==EINPROGRESS)  no SSL  
*/  
int nbTlsConnectNonBlocking(nbTLS *tls){
  int sd,rc;
  struct sockaddr_in in_addr;
  int domain=AF_INET;
#if !defined(HPUX) && !defined(SOLARIS)
  struct timeval tv;
#endif
  struct sockaddr_un un_addr;
  char *addr=tls->uriMap[tls->uriIndex].addr;
  char *name=tls->uriMap[tls->uriIndex].name;
  unsigned short port=tls->uriMap[tls->uriIndex].port;

  if(tls->uriMap[tls->uriIndex].scheme==NB_TLS_SCHEME_UNIX){
    domain=AF_UNIX;
    if(strlen(name)>sizeof(un_addr.sun_path)){
      fprintf(stderr,"nbTlsConnectNonBlocking: Local domain socket path too long - %s\n",name);
      return(-1);
      }
    }

  sd=socket(domain,SOCK_STREAM,0);
  if(sd<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: Unable to obtain socket\n");
    return(-1);
    }
  if(domain==AF_INET){
    // 2012-02-09 eat - set SO_KEEPALIVE to prevent firewalls from closing connection for return packets
    int keepalive=1;
    memset(&in_addr,0,sizeof(in_addr));
    in_addr.sin_family=AF_INET;
    in_addr.sin_addr.s_addr=inet_addr(addr);   // host IP
    in_addr.sin_port=htons(port);              // port number
    if(setsockopt(sd,SOL_SOCKET,SO_KEEPALIVE,&keepalive,(socklen_t)sizeof(keepalive))<0) {
      fprintf(stderr,"nbTlsConnectNonBlocking: setsockopt SO_KEEPALIVE failed: %s\n",strerror(errno));
      close(sd);
      return(-1);
      }
    if(tlsTrace) fprintf(stderr,"nbTlsConnectNonBlocking: set SO_KEEPALIVE on sd=%d\n",sd);
    }
  else{
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,name);
    }

#if !defined(HPUX) && !defined(SOLARIS)
  if(tls->tlsx) tv.tv_sec=tls->tlsx->timeout;
  else tv.tv_sec=5;
  tv.tv_usec=0;
  rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
  if(rc<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
    close(sd);
    return(-1);
    }
  rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
  if(rc<0){
    fprintf(stderr,"nbTlsConnectNonBlocking: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
    close(sd);
    return(-1);
    }
#endif
  if(fcntl(sd,F_SETFL,fcntl(sd,F_GETFL)|O_NONBLOCK)){
    fprintf(stderr,"nbTlsConnectNonBlocking: connect failed: %s\n",strerror(errno));
    close(sd);
    return(-1);
    }
  if(domain==AF_INET) rc=connect(sd,(struct sockaddr*)&in_addr,sizeof(in_addr));
  else rc=connect(sd,(struct sockaddr*)&un_addr,sizeof(un_addr));
  if(rc<0){
    if(errno!=EINPROGRESS){
      fprintf(stderr,"nbTlsConnectNonBlocking: connect failed: %s\n",strerror(errno));
      close(sd);
      return(-1);
      }
    if(tlsTrace) fprintf(stderr,"nbTlsConnectNonBlocking: connecting\n");
    tls->socket=sd;
    rc=nbTlsConnected(tls);
    if(rc==0) return(4);
    return(rc);
    }
  tls->socket=sd;
  return(nbTlsConnected(tls));
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
int nbTlsConnectWithinUriCount(nbTLS *tls,int uriCount){
  int sd,rc;
  struct sockaddr_in sa;
#if !defined(HPUX) && !defined(SOLARIS)
  struct timeval tv;
#endif
  SSL *ssl=NULL;
  int uriIndex;
  struct sockaddr_un un_addr;

  if(uriCount<0 || uriCount>NB_TLS_URIMAP_BOUND){   // 2013-01-01 eat - VID 4949-0.8.13-1 checking index range
    fprintf(stderr,"nbTlsConnect: Unable to obtain socket\n");
    return(-1);
    }
  // 2010-10-25 eat 0.8.4 - was looping too far
  //for(uriIndex=0;uriIndex<tls->uriCount;uriIndex++){
  for(uriIndex=0;uriIndex<uriCount;uriIndex++){
    if(tls->uriMap[uriIndex].scheme==NB_TLS_SCHEME_UNIX){   // 2013-01-01 eat  - VID 4949-0.8.13-1 checking above
      if(strlen(tls->uriMap[uriIndex].name)>=sizeof(un_addr.sun_path)){  // 2013-01-01 eat - VID 4615-0.8.13-1
        fprintf(stderr,"nbTlsListen: Local domain socket path too long - %s\n",tls->uriMap[uriIndex].name); // 2013-01-01 eat - VID 4612-0.8.13-1
        continue;
        }
      sd=socket(AF_UNIX,SOCK_STREAM,0);
      if(sd<0){
        fprintf(stderr,"nbTlsConnect: Unable to obtain socket\n");
        return(-1);
        }
      un_addr.sun_family = AF_UNIX;
      strcpy(un_addr.sun_path,tls->uriMap[uriIndex].name); // 2013-01-01 eat - VID 5041-0.8.13-1 checking above
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
      sa.sin_addr.s_addr=inet_addr(tls->uriMap[uriIndex].addr);   // host IP      // 2013-01-01 eat - VID 4843-0.8.13-1 checking above
      sa.sin_port=htons(tls->uriMap[uriIndex].port);              // port number  // 2013-01-01 eat - VID 4952-0.8.13-1 checking above
    
#if !defined(HPUX) && !defined(SOLARIS)
      if(tls->tlsx) tv.tv_sec=tls->tlsx->timeout;
      else tv.tv_sec=5;
      tv.tv_usec=0;
      rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
      if(rc==-1){
        fprintf(stderr,"nbTlsConnect: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
        close(sd);   // 2012-12-18 eat - CID 751610
        return(-1);
        } 
      rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
      if(rc==-1){
        fprintf(stderr,"nbTlsConnect: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
        close(sd);
        return(-1);
        } 
#endif
      rc=connect(sd,(struct sockaddr*)&sa,sizeof(sa));
      }
    if(rc<0) close(sd);
    else{
      tls->uriIndex=uriIndex;
      // 2010-10-21 eat 0.8.5 - close current socket if open
      if(tls->socket) close(tls->socket),tls->socket=0;
      tls->socket=sd;
      tls->ssl=ssl;
      // 2012-05-22 eat - ending this experimenting - had code depending on it
      // 2012-05-15 eat - experimenting without handle on tlsx
      if(tls->tlsx) tls->handle=tls->tlsx->handle;
      return(nbTlsConnected(tls));
      }
    }
  if(uriCount<tls->uriCount && tls->socket) return(0);  // keeping the connection we have
  fprintf(stderr,"nbTlsConnect: connect failed: %s\n",strerror(errno));
  return(-1);
  }

int nbTlsConnect(nbTLS *tls){
  return(nbTlsConnectWithinUriCount(tls,tls->uriCount));
  }

int nbTlsReconnectIfBetter(nbTLS *tls){
  if(tls->uriIndex) return(nbTlsConnectWithinUriCount(tls,tls->uriIndex));
  return(0);
  }

int nbTlsGetUriIndex(nbTLS *tls){
  return(tls->uriIndex);
  }

/*
*  SSL handshake with nonblocking errors allowed
*    nbTlsConnectHandshake was nbTlsHandshakeNonBlocking
*/
int nbTlsConnectHandshake(nbTLS *tls){
  int rc,error;
  nbTLSX *tlsx=tls->tlsx;
  SSL *ssl=NULL;

  if(tlsTrace) fprintf(stderr,"nbTlsConnectHandshake: tls->option=%d tls->tlsx=%p tls->ssl=%p\n",tls->option,tls->tlsx,tls->ssl);
  if(!tls->option) return(0);
  if(!tlsx){
    fprintf(stderr,"nbTlsConnectHandshake: Logic error - should not be called with null tlsx - terminating\n");
    exit(1);
    }
  // Seems like the tls->ssl should be set before calling this function
  // we are handling it here if not until the callers are fixed
  if(!tls->ssl){ 
    if(tlsTrace) fprintf(stderr,"nbTlsConnectHandshake: tls->tlsx->ctx=%p\n",tlsx->ctx);
    ssl=SSL_new(tlsx->ctx);
    if(!ssl){
      fprintf(stderr,"nbTlsHandshakeNonBLocking: SSL_new failed\n");
      return(-1);
      }
    SSL_set_fd(ssl,tls->socket);
    tls->ssl=ssl;
    }
  if(tlsTrace) fprintf(stderr,"nbTlsConnectHandshake: tls->socket=%u ssl=%p\n",tls->socket,tls->ssl);
  rc=SSL_connect(tls->ssl);
  if(rc!=1){
    error=SSL_get_error(tls->ssl,rc); // get error code
    if(error==SSL_ERROR_WANT_WRITE || error==SSL_ERROR_WANT_READ) return(error);
    fprintf(stderr,"nbTlsConnectHandshake: SSL_connect failed - rc=%d code=%d\n",rc,error);
    fprintf(stderr,"nbTlsConnectHandshake: Handshake failed - rc=%d code=%d\n",rc,error);
    fprintf(stderr,"SSL_ERROR_NONE = %d\n",SSL_ERROR_NONE);
    fprintf(stderr,"SSL_ERROR_ZERO_RETURN = %d\n",SSL_ERROR_ZERO_RETURN);
    fprintf(stderr,"SSL_ERROR_WANT_WRITE = %d\n",SSL_ERROR_WANT_WRITE);
    fprintf(stderr,"SSL_ERROR_WANT_READ = %d\n",SSL_ERROR_WANT_READ);
    fprintf(stderr,"SSL_ERROR_WANT_CONNECT = %d\n",SSL_ERROR_WANT_CONNECT);
#if defined(SSL_ERROR_WANT_ACCEPT)  // 2012-09-29 eat - conditional for older versions of openssl
    fprintf(stderr,"SSL_ERROR_WANT_ACCEPT = %d\n",SSL_ERROR_WANT_ACCEPT);
#endif
    fprintf(stderr,"SSL_ERROR_WANT_X509_LOOKUP = %d\n",SSL_ERROR_WANT_X509_LOOKUP);
    fprintf(stderr,"SSL_ERROR_SYSCALL = %d\n",SSL_ERROR_SYSCALL);
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

  memset(&in_addr,0,sizeof(in_addr));
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
  if(fcntl(sd,F_SETFD,FD_CLOEXEC)){
    fprintf(stderr,"nbTlsListen: Unable set close on exec flag - %s\n",strerror(errno));
    close(sd);
    return(-1);
    }
#endif
  if(domain==AF_INET){
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
    mode_t mask;
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,name);
    if(unlink(name)){  // 2013-01-01 eat - CID 762050 - this is not a problem  // 2013-01-01 eat - VID 5417-0.8.13-1
      fprintf(stderr,"nbTlsListen: Warning - Unable to unlink %s before bind - %s",name,strerror(errno));
      }
    mask=umask(0); // save umask
    fchmod(sd,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);  // set file permissions to 660
    if(bind(sd,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      umask(mask); // restore umask
      fprintf(stderr,"nbTlsListen: Unable to bind to local domain socket %s - %s\n",name,strerror(errno));
      close(sd);
      return(-1);
      }
    umask(mask); // restore umask
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
*  Issue SSL_accept for SSL handshake
*
*  Return: (like SSL_accept)
*
*    1 - success
*    0 - connection was shutdown
*   -1 - error 
*        tls->error
*          NB_TLS_ERROR_UNKNOWN
*          NB_TLS_ERROR_WANT_WRITE
*          NB_TLS_ERROR_WANT_READ
*
*/
int nbTlsAcceptHandshake(nbTLS *tls){
  int rc,error;
  char errbuf[121];

  if(tlsTrace) fprintf(stderr,"nbTlsAcceptHandshake: Issuing SSL_accept on socket %d\n",tls->socket);
  tls->error=NB_TLS_ERROR_UNKNOWN;
  rc=SSL_accept(tls->ssl);
  if(rc>0){
    if(tlsTrace) fprintf(stderr,"nbTlsAcceptHandshake: SSL connection using %s\n",SSL_get_cipher(tls->ssl));
    return(rc);   // 2011-05-25 eat - changed from rc>=0 because wee need to the error
    }
  error=SSL_get_error(tls->ssl,rc);
  if(error==SSL_ERROR_WANT_READ) tls->error=NB_TLS_ERROR_WANT_READ;
  else if(error==SSL_ERROR_WANT_WRITE) tls->error=NB_TLS_ERROR_WANT_WRITE;
  else{
    fprintf(stderr,"nbTlsAcceptHandshake: SSL_accept rc=%d code=%d\n",rc,error);
    fprintf(stderr,"nbTlsAcceptHandshake: %s\n",ERR_error_string(error,errbuf));
    ERR_print_errors_fp(stderr);
    tls->error=0;
    }
  return(rc);
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
*    That must be implemented at a higher level.
*
*  Returns nbTLS pointer or NULL
*
*    When using non-blocking IO, the tls->error code must be checked
*    for NB_TLS_ERROR_WANT_WRITE or NB_TLS_ERROR_WANT_READ.  On these
*    error codes schedule calls to nbTlsAcceptHandshake when the
*    write or read can be satisified.
*
*    When a NULL value is returned, the error value on the TLS listener
*    is set to NB_TLS_ERROR_HANDSHAKE to indicate a handshake error
*    distinct from a more fundamental socket error.
*/
nbTLS *nbTlsAccept(nbTLS *tlsListener){
  nbTLS *tls;
#if !defined(HPUX) && !defined(SOLARIS)
  struct timeval tv;
#endif
  int sd,rc;
  char *protocol;
  struct sockaddr_in client;
  int keepalive=1;
#if defined(WIN32) || defined(mpe) || defined(SOLARIS) || defined(MACOS)
  size_t sockaddrlen;
#else
  socklen_t sockaddrlen;
#endif
  int socket=tlsListener->socket;
  sockaddrlen=sizeof(client);

  tlsListener->error=0; // clear error
#if defined(MACOS)
  sd=accept(socket,(struct sockaddr *)&client,(int *)&sockaddrlen);
#else
  sd=accept(socket,(struct sockaddr *)&client,&sockaddrlen);
#endif
  if(sd<0){
    if(errno!=EINTR) fprintf(stderr,"nbTlsAccept: accept failed - %s\n",strerror(errno));
    return(NULL);
    }
#if !defined(WIN32)
  if(fcntl(sd,F_SETFD,FD_CLOEXEC)){
    fprintf(stderr,"nbTlsAccept: set flag close-on-exec failed: %s\n",strerror(errno));
    close(sd);
    return(NULL);
    }
  // 2011-02-05 eat - if listener is non-blocking, make new socket non-blocking
  if(fcntl(socket,F_GETFL)&O_NONBLOCK){
    if(fcntl(sd,F_SETFL,fcntl(sd,F_GETFL)|O_NONBLOCK)){
      fprintf(stderr,"nbTlsAccept: set flag non-blocking failed: %s\n",strerror(errno));
      close(sd);
      return(NULL);
      }
    }
  if(setsockopt(sd,SOL_SOCKET,SO_KEEPALIVE,&keepalive,(socklen_t)sizeof(keepalive))<0) {
    fprintf(stderr,"nbTlsAccept: setsockopt SO_KEEPALIVE failed: %s\n",strerror(errno));
    close(sd);
    return(NULL);
    }
  if(tlsTrace) fprintf(stderr,"nbTlsAccept: set SO_KEEPALIVE on sd=%d\n",sd);

#endif
#if !defined(HPUX) && !defined(SOLARIS)
  if(tlsListener->tlsx) tv.tv_sec=tlsListener->tlsx->timeout;
  else tv.tv_sec=5;
  tv.tv_usec=0;
  rc=setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(void *)&tv,sizeof(tv));
  if(rc==-1){
    fprintf(stderr,"nbTlsAccept: setsockopt SO_RCVTIMEO failed: %s\n",strerror(errno));
    close(sd);
    return(NULL);
    }
  rc=setsockopt(sd,SOL_SOCKET,SO_SNDTIMEO,(void *)&tv,sizeof(tv));
  if(rc==-1){
    fprintf(stderr,"nbTlsAccept: setsockopt SO_SNDTIMEO failed: %s\n",strerror(errno));
    close(sd);
    return(NULL);
    }
#endif
  // Create the nbTLS structure
  // 2012-10-13 eat - replaced malloc
  tls=nbAlloc(sizeof(nbTLS));
  memset(tls,0,sizeof(nbTLS));
  if(tlsTrace) fprintf(stderr,"nbTlsAccept: tlsListener->tlsx=%p\n",tlsListener->tlsx);
  if(tlsListener->tlsx) tls->option=tlsListener->tlsx->option,tls->uriMap[0].scheme=NB_TLS_SCHEME_TLS;
  else tls->option=NB_TLS_OPTION_TCP,tls->uriMap[0].scheme=NB_TLS_SCHEME_TCP;
  tls->socket=sd;
  tls->uriCount=1;
  tls->tlsx=tlsListener->tlsx;
  if(client.sin_family==AF_UNIX){
    tls->uriMap[0].scheme=NB_TLS_SCHEME_UNIX;
    strcpy(tls->uriMap[0].name,tlsListener->uriMap[0].name);
    }
  else{
#if defined(mpe)
    strcpy(tls->uriMap[0].addr,(char *)inet_ntoa(client.sin_addr));
#else
    strncpy(tls->uriMap[0].addr,inet_ntoa(client.sin_addr),sizeof(tls->uriMap[0].addr)-1); // 2014-01-25 eat - CID 1164440
    *(tls->uriMap[0].addr+sizeof(tls->uriMap[0].addr)-1)=0;
#endif
    tls->uriMap[0].port=ntohs(client.sin_port);
    }
  if(tlsTrace) fprintf(stderr,"nbTlsAccept: tls->option=%d\n",tls->option);
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

    if(tlsTrace) fprintf(stderr,"nbTlsAccept: Issuing SSL_accept on socket %d\n",sd);
    rc=nbTlsAcceptHandshake(tls);
    if(rc==0 || (rc==-1 && tls->error!=NB_TLS_ERROR_WANT_READ && tls->error!=NB_TLS_ERROR_WANT_WRITE)){
      tlsListener->error=NB_TLS_ERROR_HANDSHAKE; 
      nbTlsFree(tls);
      return(NULL);
      }
    if(tlsTrace) fprintf(stderr,"nbTlsAccept: SSL connection using %s\n",SSL_get_cipher(tls->ssl));
    }
  else{
    if(tlsTrace) fprintf(stderr,"nbTlsAccept: using clear tcp instead of tls\n");
    protocol="tcp";
    tls->ssl=NULL;
    }
  if(tls->uriMap[0].scheme==NB_TLS_SCHEME_UNIX)
    snprintf(tls->uriMap[0].uri,sizeof(tls->uriMap[0].uri),"unix://%s",tls->uriMap[0].name);  // 2013-01-12 eat VID 6426-0.8.13-2
  else snprintf(tls->uriMap[0].uri,sizeof(tls->uriMap[0].uri),"%s://%s:%d",protocol,tls->uriMap[0].addr,tls->uriMap[0].port); // 2013-01-12 eat - VID 6404-0.8.13-2
  // 2012-05-22 eat - ending the experimenting - had code depending on it
  // 2012-05-15 eat - experimenting without handle on tlsx
  if(tls->tlsx) tls->handle=tls->tlsx->handle;
  return(tls);
  }

/*
*  Read from peer
*
*  When -1 is returned, the caller should handle the error code set
*  in the nbTLS structure (tls->error).
*
*    NB_TLS_ERROR_WANT_READ
*    NB_TLS_ERROR_WANT_WRITE  - possible if SSL is handshaking again
*    NB_TLS_ERROR_UNKNOWN 
*/
int nbTlsRead(nbTLS *tls,char *buffer,size_t size){
  ssize_t len;
  int error;
  if(tlsTrace) fprintf(stderr,"nbTlsRead: size=%d\n",(int)size);
  tls->error=NB_TLS_ERROR_UNKNOWN;
  if(!tls->ssl){
    if(tlsTrace) fprintf(stderr,"nbTlsRead: calling clear recv\n");
    len=recv(tls->socket,buffer,size,0); // 2013-01-01 eat - VID 5048-0.8.13-1 Intentional - user provide buffer and size
    while(len==-1 && errno==EINTR) len=recv(tls->socket,buffer,size,0); // 2013-01-01 eat - VID 5131-0.8.13-1 Intentional 
    if(tlsTrace) fprintf(stderr,"nbTlsRead: read len=%ld\n",len);
    if(len==-1 && errno==EAGAIN) tls->error=NB_TLS_ERROR_WANT_READ;
    return(len);
    }
  if(tlsTrace) fprintf(stderr,"nbTlsRead: calling SSL_read\n");
  len=SSL_read(tls->ssl,buffer,size);
  if(len<0){
    error=SSL_get_error(tls->ssl,len);
    fprintf(stderr,"nbTlsRead: SSL_read rc=%ld code=%d sd=%d\n",len,error,tls->socket);
    ERR_print_errors_fp(stderr);
    if(error==SSL_ERROR_WANT_READ) tls->error=NB_TLS_ERROR_WANT_READ;
    else if(error==SSL_ERROR_WANT_WRITE) tls->error=NB_TLS_ERROR_WANT_WRITE;
    }
  if(tlsTrace) fprintf(stderr,"nbTlsRead: SSL_read len=%ld\n",len);
  return(len);
  }

/*
*  Write to peer
*
*  When -1 is returned, the caller should handle the error code set
*  in the nbTLS structure (tls->error).
*
*    NB_TLS_ERROR_WANT_READ   - possible if SSL is handshaking again
*    NB_TLS_ERROR_WANT_WRITE
*    NB_TLS_ERROR_UNKNOWN 
*/
int nbTlsWrite(nbTLS *tls,char *buffer,size_t size){
  ssize_t len;
  int error;
  if(tlsTrace) fprintf(stderr,"nbTlsWrite: size=%zu\n",size);
  tls->error=NB_TLS_ERROR_UNKNOWN;
  if(!tls->ssl){
    if(tlsTrace) fprintf(stderr,"nbTlsWrite: calling clear send - socket=%d\n",tls->socket);
    len=send(tls->socket,buffer,size,0);   // 2013-01-01 eat - VID 4945-0.8.13-1 FP
    while(len==-1 && errno==EINTR) len=send(tls->socket,buffer,size,0); // 2013-01-01 eat - VID 5269-0.8.13 FP
    if(tlsTrace) fprintf(stderr,"nbTlsWrite: wrote len=%ld\n",len);
    if(len==-1 && errno==EAGAIN) tls->error=NB_TLS_ERROR_WANT_WRITE;
    return(len);
    }
  if(tlsTrace) fprintf(stderr,"nbTlsWrite: calling SSL_write - ssl=%p\n",tls->ssl);
  len=SSL_write(tls->ssl,buffer,size);
  if(len<0){
    error=SSL_get_error(tls->ssl,len);
    fprintf(stderr,"nbTlsWrite: SSL_write rc=%ld code=%d sd=%d\n",len,error,tls->socket);
    ERR_print_errors_fp(stderr);
    if(error==SSL_ERROR_WANT_READ) tls->error=NB_TLS_ERROR_WANT_READ;
    else if(error==SSL_ERROR_WANT_WRITE) tls->error=NB_TLS_ERROR_WANT_WRITE;
    }
  if(tlsTrace) fprintf(stderr,"nbTlsWrite: SSL_write len=%ld\n",len);
  return(len);
  }

/*
*  Close a TLS connection and free the structure
*/
int nbTlsClose(nbTLS *tls){
  int rc=0;

  if(!tls) return(0);
  if(tls->ssl){
    SSL_shutdown(tls->ssl);
    SSL_free(tls->ssl);
    tls->ssl=NULL;
    }
  if(tls->socket){
#if defined(WIN32)
    shutdown(tls->socket,SD_BOTH);
    rc=closesocket(tls->socket);
#else
    shutdown(tls->socket,SHUT_RDWR);
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
  nbFree(tls,sizeof(nbTLS));
  return(0);
  }

//==========================================================================
// Functions that require a NodeBrain context
//==========================================================================

/*
*  Create a TLS/SSL context from NodeBrain glossary
*/
nbTLSX *nbTlsLoadContext(nbCELL context,nbCELL tlsContext,void *handle,int client){
  nbTLSX *tlsx;
  char *optionStr,*keyFile="",*certFile="",*trustedCertsFile="";
  int option=0,timeout;

  nbLogMsg(context,0,'T',"nbTlsLoadContext: checking for tls options");
  optionStr=nbTermOptionString(tlsContext,"option","CERTS");
  if(strcmp(optionStr,"certs")==0) option=NB_TLS_OPTIONS_CERTS|NB_TLS_OPTION_SSL2;
  else if(strcmp(optionStr,"cert")==0) option=NB_TLS_OPTIONS_CERT|NB_TLS_OPTION_SSL2;
  else if(strcmp(optionStr,"keys")==0) option=NB_TLS_OPTIONS_KEYS|NB_TLS_OPTION_SSL2;
  else if(strcmp(optionStr,"tls")==0) option=NB_TLS_OPTIONS_CERT|NB_TLS_OPTION_SSL2; 
  else if(strcmp(optionStr,"tcp")==0) option=NB_TLS_OPTIONS_TCP;
  // new options
  else if(strcmp(optionStr,"CERTS")==0) option=NB_TLS_OPTIONS_CERTS;
  else if(strcmp(optionStr,"CERTO")==0) option=NB_TLS_OPTIONS_CERTO;
  else if(strcmp(optionStr,"CERT")==0) option=NB_TLS_OPTIONS_CERT;
  else if(strcmp(optionStr,"KEYS")==0) option=NB_TLS_OPTIONS_KEYS;
  else if(strcmp(optionStr,"ADH")==0) option=NB_TLS_OPTIONS_TLS;
  else if(strcmp(optionStr,"TCP")==0) option=NB_TLS_OPTIONS_TCP;
  else{
    nbLogMsg(context,0,'E',"nbTlsLoadContext: Option '%s' not recognized.",optionStr);
    return(NULL);
    }
  if(client) option|=NB_TLS_OPTION_CLIENT;
  else option|=NB_TLS_OPTION_SERVER;
  timeout=nbTermOptionInteger(tlsContext,"timeout",5);
  if(option&NB_TLS_OPTION_TLS){
    //if(option&(NB_TLS_SERVER_CERTS|NB_TLS_SERVER_CERT|NB_TLS_SERVER_KEYS|NB_TLS_SERVER_TLS)){
    if(option&NB_TLS_OPTION_CERTS || (option&NB_TLS_OPTION_SERVER && option&NB_TLS_OPTION_CERT)){
      keyFile=nbTermOptionString(tlsContext,"keyfile","security/ServerKey.pem");
      certFile=nbTermOptionString(tlsContext,"certfile","security/ServerCertificate.pem");
      }
    if(option&NB_TLS_OPTION_CERT)
      trustedCertsFile=nbTermOptionString(tlsContext,"trustfile","security/TrustedCertificates.pem");
    else if(option&NB_TLS_OPTION_SERVER)
      keyFile=nbTermOptionString(tlsContext,"dhfile","security/ServerAnonymousKey.pem");
    tlsx=nbTlsCreateContext(option,handle,timeout,keyFile,certFile,trustedCertsFile);
    nbLogMsg(context,0,'T',"nbTlsLoadContext: returning tlsx=%p",tlsx);
    return(tlsx); 
    }
  return(NULL);
  }

nbTLS *nbTlsLoadListener(nbCELL context,nbCELL tlsContext,char *defaultUri,void *handle){
  nbTLS *tls;
  nbTLSX *tlsx=NULL;
  char *uri;

  if(tlsTrace) nbLogMsg(context,0,'T',"nbTlsLoadListener: called");
  uri=nbTermOptionString(tlsContext,"uri",defaultUri);
  if(strncmp(uri,"tls:",4)==0 || strncmp(uri,"https:",6)==0) tlsx=nbTlsLoadContext(context,tlsContext,handle,0);
  tls=nbTlsCreate(tlsx,uri);
  if(!tls){
    nbLogMsg(context,0,'E',"nbTlsListener: Syntax error in uri=\"%s\"",uri);
    return(NULL); 
    }
  else nbLogMsg(context,0,'T',"nbTlsListener: Parsed uri=\"%s\"",uri);
  if(nbTlsListen(tls)<0){
    nbLogMsg(context,0,'E',"nbTlsListener: Unable to start listener - %s",tls->uriMap[0].uri);
    }
  if(tlsTrace) nbLogMsg(context,0,'T',"nbTlsLoadListener: uri=\"%s\" sd=%d",uri,tls->socket);
  return(tls);
  }


/*
*  Schedule a non-blocking connection
*
*  Returns: 
*    -1 - error - see errno
*     0 - connecting
*     1 - connection made immediately and handler called
*/
int nbTlsConnectNonBlockingAndSchedule(nbCELL context,nbTLS *tls,void *handle,void (*handler)(nbCELL context,int sd,void *handle)){
  int rc;

  if(tlsTrace){
    nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: called");
    //nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls=%p",tls);
    //nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriIndex=%d",tls->uriIndex);
    nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriMap[tls->uriIndex].uri=%s name=%s addr=%s port=%d",tls->uriMap[tls->uriIndex].uri,tls->uriMap[tls->uriIndex].name,tls->uriMap[tls->uriIndex].addr,tls->uriMap[tls->uriIndex].port);
    }
  rc=nbTlsConnectNonBlocking(tls);
  if(rc==1){
    if(tlsTrace) nbLogMsg(context,0,'I',"Success on non-blocking connect sd=%d %s",tls->socket,tls->uriMap[tls->uriIndex].uri);
    (*handler)(context,tls->socket,handle);
    }
  else if(rc==0){
    if(tlsTrace) nbLogMsg(context,0,'I',"nbTlsConnectNonBlockingAndSchedule: in progress - sd=%d %s",tls->socket,tls->uriMap[tls->uriIndex].uri);
    nbListenerAddWrite(context,tls->socket,handle,handler); 
    } 
  else nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: connect failed - %s - %s",tls->uriMap[tls->uriIndex].uri,strerror(errno));
  return(rc);
  }
