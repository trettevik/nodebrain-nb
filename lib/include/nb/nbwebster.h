/*
* Copyright (C) 2007-2013 The Boeing Company
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbwebster.h
*
* Title:    NodeBrain Web Server API
*
* Function:
*
*   This header defines structures and routines used in the web server API.
*
* See nbwebster.c for more information       
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2007-07-27 Ed Trettevik (version 0.6.8 prototype in Webster module)
* 2011-05-06 eat 0.8.5 - restructured as API for use by other modules
* 2011-10-16 eat 0.8.6 - included resource filters
* 2011-10-22 eat 0.8.6 - included server config attribute
* 2012-02-16 eat 0.8.7 - included nb_websterTrace
* 2012-05-11 eat 0.8.9 - replace keepAlive flag with close flag
* 2012-05-16 eat 0.8.9 - include forwardTlsx and forwardUri for reuse
* 2012-12-30 eat 0.8.13 Included nbWebsterGetRootDir function
*==============================================================================
*/

//=============================================================================
// Old Structures
//   These structures will be referenced by new structures and eventually
//   eliminated as their functionality will be replaces by the new structures.
//
// Note: 
//   These structures and related code come from the Webster module nb_webster.c
//   and will most likely remain in Webster, but with many fields and functions 
//   being replaced by this API.
//=============================================================================

extern int nb_websterTrace;       // debugging trace flag for webster routines

typedef struct NB_WEB_USER{   // Webster User Structure
  struct NB_TREE_NODE node;       // Binary tree node
  char             userid[32];    // user identifier
  char             role;          // 0 - read only, 1 - cgi user, 2 - administrator
  } nbWebUser;

#define NB_WEBSTER_ROLE_REJECT 0
#define NB_WEBSTER_ROLE_GUEST  1
#define NB_WEBSTER_ROLE_ADMIN  255

#define NB_WEBSTER_METHOD_GET   1
#define NB_WEBSTER_METHOD_POST  2

//=============================================================================
// New Structures
//=============================================================================
typedef struct NB_WEB_SERVER{      // Web Server
  nbCELL           context;        // node context
  nbCELL           siteContext;    // site context within node context
  void            *handle;         // handle for application structure
  void *(*handler)(nbCELL context,void *handle,int operation);  // session handler
  char            *uri;            // URI for server 
  struct NB_PROXY *server;         // listening server proxy
  struct NB_WEB_RESOURCE *resource;// registered resources
  struct NB_WEB_SESSION  *session; // tree of active sessions
  nbCELL           forwardContext; // forwarding context when proxying
  char            *forwardUri;     // forwarding URI
  nbTLSX          *forwardTlsx;    // forwarding TLSX (openssl context)
  char            *rootdir;        // web site root directory
  char            *indexPage;      // web site root directory
  char            *indexQuery;     // web site root directory
  char            *authenticate;   // "yes" | "certificate" | "password" | "no"
  char             dir[1024];      // working directory path - NodeBrain caboodle
  nbWebUser       *userTree;       // List of authorized users
  nbCELL           filter;         // nodebrain translator (request filter)
  char            *config;         // configuration file name
  } nbWebServer;

typedef struct NB_WEB_RESOURCE{   // Tree of cached resources
  NB_TreeNode      node;          // binary tree node
  NB_TreeNode     *child;         // subordinate nodes
  void            *handle;        // handle (points to reply when handler is built-in cache handler)
  int (*handler)(nbCELL context,struct NB_WEB_SESSION *session,void *handle);
  } nbWebResource;
  
typedef struct NB_WEB_SESSION{    // Web Session
  struct NB_TREE_NODE node;       // Binary tree node
  void            *handle;        // session handle
  char            *cookiesIn;     // cookies from client
  char            *cookiesOut;    // cookies to include in header fields
  int              expires;       // reply "Expires" header time as offset from current time in seconds
  unsigned char    cookie[256];   // encrypted cookie for session control
  struct NB_PROXY *client;        // connection to client
  struct NB_PROXY *server;        // connection to server (when acting as a proxy server)
  nbProxyBook      book;          // temporary book
  int              fd;            // file descriptor (reading content files)
  //int              cgiClosed;     // flag indicated withn CGI program ended
  //nbCELL           userIdCell;    // Cell containing user
  //char            *userId;        // User ID
  char             email[320];    // email address from certificate (optional)
  char            *roleName;      // Role name
  char            *type;          // set to constant by nbWebsterSetType - default "text"
  char            *subtype;       // set to constant by nbWebsterSetType - default "html"
  // old section
  nbWebServer     *webster;       // webster server structure
  nbPROCESS        process;        // CGI process
  unsigned char    role;          // user role - see NB_WEBSTER_ROLE_* below
  char             method;        // request method - see NB_WEBSTER_METHOD_* below
  //char             keepAlive;     // keep connection for future request
  char             close;         // close connection after responding to request
  char             reqcn[128];    // X509 certificate common name for valid certificate
  char             reqhost[512];  // request host
  char             reqauth[512];  // request authentication (basic - base64 encoded "user:password"
  char             userid[64];    // user id
  char            *resource;      // requested resource - points into request
  char            *queryString;   // query string - points into request
  char            *headerfields;  // http message header fields - points into request
                                  // NOTE: content and contentLength are temporary until passed to CGI program
  char            *content;       // content for CGI Post - remaining
  int              contentLength; // content length for CGI Post - remaining
  char             request[NB_BUFSIZE];     // request buffer
  char             command[NB_BUFSIZE];     // medulla command buffer
  char             parameters[NB_BUFSIZE];  // decoded parameter buffer
  } nbWebSession;

// API functions
//extern nbWebServer *nbWebsterOpen(nbCELL context,void *handle,char *interface,short int port);
extern nbWebServer *nbWebsterOpen(nbCELL context,nbCELL siteContext,void *handle,
  void *(*handler)(nbCELL context,void *handle,int operation));
extern int nbWebsterEnable(nbCELL context,nbWebServer *webster);
extern int nbWebsterDisable(nbCELL context,nbWebServer *webster);
extern nbWebServer *nbWebsterClose(nbCELL context,nbWebServer *webster);
extern int nbWebsterRegisterResource(nbCELL context,nbWebServer *webster,char *name,void *handle,
  int (*handler)(nbCELL context,struct NB_WEB_SESSION *session,void *handle));
extern nbWebResource *nbWebsterFindResource(nbCELL context,nbWebServer *webster,char *name);
extern char *nbWebsterGetConfig(nbCELL context,nbWebServer *webster);
extern char *nbWebsterGetRootDir(nbCELL context,nbWebServer *webster);

extern void *nbWebsterGetHandle(nbCELL context,nbWebSession *session);
extern void *nbWebsterGetSessionHandle(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetHost(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetDir(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetResource(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetQuery(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetParameters(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetParam(nbCELL context,nbWebSession *session,char *param);
extern char *nbWebsterGetParamNext(nbCELL context,nbWebSession *session,char *param,char **pCursor);
extern int *nbWebsterPut(nbCELL context,nbWebSession *session,void *data,int len);
extern int *nbWebsterPutText(nbCELL context,nbWebSession *session,char *text);
extern int *nbWebsterReply(nbCELL context,nbWebSession *session);
extern char *nbWebsterGetCookies(nbCELL context,nbWebSession *session);
extern void nbWebsterSetCookies(nbCELL context,nbWebSession *session,char *cookies);
extern void nbWebsterSetType(nbCELL context,nbWebSession *session,char *type,char *subtype);
extern void nbWebsterSetExpires(nbCELL context,nbWebSession *session,int seconds);
extern char *nbWebsterParameterEncode(nbCELL context,nbWebSession *session,char *plain,char *encoded,int len);
extern char *nbWebsterParameterDecode(nbCELL context,nbWebSession *session,char *encoded,char *plain,int len);

