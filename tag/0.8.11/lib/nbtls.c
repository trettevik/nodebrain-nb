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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program: NodeBrain
*
* File:    nbtls.c
*
* Title:   NodeBrain Transport Layer Security
*
* Purpose:
*
*   This file provides functions for Transport Layer Security
*   within a NodeBrain environment.  The functions provided
*   in this file augment the nb-tls.c functions that are
*   designed to be independent of a NodeBrain environment.
*
* Description:
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
*  If file
*   is NULL or the empty string, the context is searched as is.  Otherwise the
*   file is first sourced into the context.
*
*     nbTLSX nbTlsCreateContext(nbCELL context,char *file);
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2009-06-01 Ed Trettevik (version 0.7.7)
* 2010-06-06 eat 0.8.2  included client parameter to nbTlsLoadContext
* 2010-06-16 eat 0.8.2  put debug messages under tlsTrace option
*=============================================================================
*/
#include <nb.h>
#include <nb-tls.h>
#include <nbtls.h>

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
  nbTLSX *tlsx;
  char *uri;

  if(tlsTrace) nbLogMsg(context,0,'T',"nbTlsLoadListener: called");
  tlsx=nbTlsLoadContext(context,tlsContext,handle,0);
  uri=nbTermOptionString(tlsContext,"uri",defaultUri);
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
