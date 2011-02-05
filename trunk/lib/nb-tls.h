#ifndef _NB__TLS_H_
#define _NB__TLS_H_

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

typedef struct NB_TLS_HANDLE{   // TLS Handle
  int option;                   // see NB_TLS_OPTION_*
  int socket;                   // socket
  int error;                    // last error code
  unsigned char uriIndex;       // uri we are using
  unsigned char uriCount;       // number of uri values
  nbTlsUriMap uriMap[4];        // uri mapping - this will replace the next 4 attributes
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
#define NB_TLS_OPTION_CLIENT 16  // Server and client certificates

// Server options

#define NB_TLS_SERVER_TCP     0  // anonymous unencrypted
#define NB_TLS_SERVER_TLS     1  // encrypted
#define NB_TLS_SERVER_KEYS    3  // Shared keys checked after TLS handshake
#define NB_TLS_SERVER_CERT    5  // Server certificate
#define NB_TLS_SERVER_CERTS  13  // Server and client certificates

// Client options

#define NB_TLS_CLIENT_TCP    16  // anonymous unencrypted
#define NB_TLS_CLIENT_TLS    17  // encrypted
#define NB_TLS_CLIENT_KEYS   19  // Shared keys checked after TLS handshake
#define NB_TLS_CLIENT_CERT   21  // Server certificate
#define NB_TLS_CLIENT_CERTS  29  // Server and client certificates

// Error codes

#define NB_TLS_ERROR_UNKNOWN    0  // Unknown error - check errno or SSL_get_error
#define NB_TLS_ERROR_WANT_WRITE 1  // Non-blocking - reschedule write
#define NB_TLS_ERROR_WANT_READ  2  // Non-blocking - reschedule read

// API Functions

extern int tlsTrace;          // debugging trace flag for TLS routines

extern nbTLSX *nbTlsCreateContext(int option,void *handle,int timeout,char *keyFile,char *certFile,char *trustedCertsFile);
extern int nbTlsFreeContext(nbTLSX *tlsx);

extern int nbTlsUriParse(nbTlsUriMap *uriMap,int n,char *uri);

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


#endif
