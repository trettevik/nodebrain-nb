/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbtls.h
*
* Title:    Transport Layer Security Header
*
* Purpose:
*
*   This file provides function headers for Transport Layer Security
*   using the OpenSSL library.
*
* See nbtls.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2009-12-12 eat 0.7.7  (original prototype)
* 2011-02-08 eat 0.8.5  Included nbTlsGetUri
*=============================================================================
*/

#ifndef _NB_TLS_H_
#define _NB_TLS_H_

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct NB_TLS_URI_MAP{      // URI list parsing structure - array is used for list
  char  uri[128];
  int   scheme;
  char  name[128];
  char  addr[16];
  int   port;
  } nbTlsUriMap;
  
#define NB_TLS_SCHEME_FILE   1
#define NB_TLS_SCHEME_UNIX   2
#define NB_TLS_SCHEME_TCP    3
#define NB_TLS_SCHEME_TLS    4
#define NB_TLS_SCHEME_HTTPS  5

typedef struct NB_TLS_CONTEXT{  // TLS Context
  int option;                   // see NB_TLS_OPTION_*
  int timeout;
  SSL_CTX *ctx;
  void *handle;                 // user data handle
  } nbTLSX;

#define NB_TLS_URIMAP_BOUND  4

typedef struct NB_TLS_HANDLE{   // TLS Handle
  int option;                   // see NB_TLS_OPTION_*
  int socket;                   // socket
  int error;                    // last error code
  unsigned char uriIndex;       // uri we are using
  unsigned char uriCount;       // number of uri values
  nbTlsUriMap uriMap[NB_TLS_URIMAP_BOUND];  // uri mapping 
  nbTLSX *tlsx;
  SSL *ssl;
  void *handle;                 // user data handle
  } nbTLS;

// Option flags passed to nbTlsContext() - use SERVER and CLIENT constants in the call
// The OPTION values are used by the API to test individual flags

#define NB_TLS_OPTION_TCP     0  // anonymous unencrypted
#define NB_TLS_OPTION_TLS     1  // encrypted
#define NB_TLS_OPTION_KEYS    2  // Shared keys checked after TLS handshake
#define NB_TLS_OPTION_CERT    4  // Server certificate
#define NB_TLS_OPTION_CERTS   8  // Server and client certificates
#define NB_TLS_OPTION_CLIENT 16  // Client
#define NB_TLS_OPTION_SERVER 32  // Server
#define NB_TLS_OPTION_CERTO  64  // Server and optional client certificates
#define NB_TLS_OPTION_SSL2  128  // Enable SSLv2 for compatibility with old applications

// Option combinations

#define NB_TLS_OPTIONS_TCP    0  // anonymous unencrypted
#define NB_TLS_OPTIONS_TLS    1  // encrypted
#define NB_TLS_OPTIONS_KEYS   3  // Shared keys checked after TLS handshake
#define NB_TLS_OPTIONS_CERT   5  // Server certificate
#define NB_TLS_OPTIONS_CERTS 13  // Server and client certificates
#define NB_TLS_OPTIONS_CERTO 77  // Server and optional client certificates

// Error codes

#define NB_TLS_ERROR_UNKNOWN    0  // Unknown error - check errno or SSL_get_error
#define NB_TLS_ERROR_WANT_WRITE 1  // Non-blocking - reschedule write
#define NB_TLS_ERROR_WANT_READ  2  // Non-blocking - reschedule read
#define NB_TLS_ERROR_HANDSHAKE  3  // Handshake error - set by nbTlsAccept on listening TLS structure when returning NULL TLS

// API Functions that do not require a NodeBrain Context

extern int tlsTrace;          // debugging trace flag for TLS routines

extern nbTLSX *nbTlsCreateContext(int option,void *handle,int timeout,char *keyFile,char *certFile,char *trustedCertsFile);
extern int nbTlsFreeContext(nbTLSX *tlsx);

extern int nbTlsUriParse(nbTlsUriMap *uriMap,int n,char *uri);

extern char *nbTlsGetUri(nbTLS *tls);

extern nbTLS *nbTlsCreate(nbTLSX *tlsx,char *uri);

extern int nbTlsListen(nbTLS *tls);

extern nbTLS *nbTlsAccept(nbTLS *tls);

extern int nbTlsAcceptHandshake(nbTLS *tls);

extern int nbTlsConnect(nbTLS *tls);

extern int nbTlsReconnectIfBetter(nbTLS *tls);

extern int nbTlsGetUriIndex(nbTLS *tls);

extern int nbTlsConnectNonBlocking(nbTLS *tls);

extern int nbTlsConnected(nbTLS *tls);

extern int nbTlsConnectHandshake(nbTLS *tls);

extern int nbTlsRead(nbTLS *tls,char *buffer,size_t size);

extern int nbTlsWrite(nbTLS *tls,char *buffer,size_t size);

extern int nbTlsClose(nbTLS *tls);

extern int nbTlsFree(nbTLS *tls);

// API Functions that require a NodeBrain context

nbTLSX *nbTlsLoadContext(nbCELL context,nbCELL tlsContext,void *handle,int client);
extern nbTLS *nbTlsLoadListener(nbCELL context,nbCELL tlsContext,char *defaultUri,void *handle);
extern int nbTlsConnectNonBlockingAndSchedule(nbCELL context,nbTLS *tls,void *handle,void (*handler)(nbCELL context,int sd,void *handle));

#endif
