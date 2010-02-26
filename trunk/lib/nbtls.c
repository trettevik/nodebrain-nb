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
* 2009/06/01 Ed Trettevik (version 0.7.7)
*=============================================================================
*/
#include <nb.h>
#include <nb-tls.h>
#include <nbtls.h>

nbTLSX *nbTlsLoadContext(nbCELL context,nbCELL tlsContext,void *handle){
  nbTLSX *tlsx;
  char *optionStr,*keyFile,*certFile,*trustedCertsFile;
  int option,timeout;

  optionStr=nbTermOptionString(tlsContext,"option","certs");
  if(strcmp(optionStr,"certs")==0) option=NB_TLS_SERVER_CERTS;
  else if(strcmp(optionStr,"cert")==0) option=NB_TLS_SERVER_CERT;
  else if(strcmp(optionStr,"keys")==0) option=NB_TLS_SERVER_KEYS;
  else if(strcmp(optionStr,"tls")==0) option=NB_TLS_SERVER_TLS;
  else if(strcmp(optionStr,"tcp")==0) option=NB_TLS_SERVER_TCP;
  else{
    nbLogMsg(context,0,'E',"nbTlsLoadContext: Option '%s' not recognized.",optionStr);
    return(NULL);
    }
  timeout=nbTermOptionInteger(tlsContext,"timeout",5);
  keyFile=nbTermOptionString(tlsContext,"keyfile","security/ServerKey.pem");
  certFile=nbTermOptionString(tlsContext,"certfile","security/ServerCertificate.pem");
  trustedCertsFile=nbTermOptionString(tlsContext,"trustfile","security/TrustedCertificates.pem");
  tlsx=nbTlsCreateContext(option,handle,timeout,keyFile,certFile,trustedCertsFile);
  return(tlsx); 
  }

//int nbTlsLoadListener(nbCELL context,int defaultPort){
//  nbTLSX *tlsx;
//  char *interface;
//  int port;
//  int sd;
//
//  interface=nbTermOptionString(context,"interface","0.0.0.0");
//  port=nbTermOptionInteger(context,"port",defaultPort); 
//  sd=nbTlsListen(interface,port);
//  nbLogMsg(context,0,'T',"interface=%s,port=%d,socket=%d",interface,port,sd);
//  return(sd);
//  } 

nbTLS *nbTlsLoadListener(nbCELL context,nbCELL tlsContext,char *defaultUri,void *handle){
  nbTLS *tls;
  nbTLSX *tlsx;
  char *uri;

  nbLogMsg(context,0,'T',"nbTlsLoadListener: called");
  tlsx=nbTlsLoadContext(context,tlsContext,handle);
  uri=nbTermOptionString(tlsContext,"uri",defaultUri);
  tls=nbTlsCreate(tlsx,uri);
  if(nbTlsListen(tls)<0){
    nbLogMsg(context,0,'E',"Unable to start listener - %s",tls->uriMap[0].uri);
    }
  nbLogMsg(context,0,'T',"nbTlsLoadListener: uri=\"%s\" sd=%d",uri,tls->socket);
  return(tls);
  }


/*
*  Schedule a non-blocking connection
*
*  Returns: 
*    -1 - error - see errno
*     0 - connect make immediately and handler called
*    >0 - socket that has been scheduled to call handler when connection is made
*/
int nbTlsConnectNonBlockingAndSchedule(nbCELL context,nbTLS *tls,void *handle,void (*handler)(nbCELL context,int sd,void *handle)){
  int rc;

  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: called");
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls=%p",tls);
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriIndex=%d",tls->uriIndex);
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriMap[tls->uriIndex].uri=%s",tls->uriMap[tls->uriIndex].uri);
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriMap[tls->uriIndex].name=%s",tls->uriMap[tls->uriIndex].name);
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriMap[tls->uriIndex].addr=%s",tls->uriMap[tls->uriIndex].addr);
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: tls->uriMap[tls->uriIndex].port=%d",tls->uriMap[tls->uriIndex].port);
  rc=nbTlsConnectNonBlocking(tls);
  if(rc==0){
    nbLogMsg(context,0,'I',"Success on non-blocking connect %s",tls->uriMap[tls->uriIndex].uri);
    (*handler)(context,tls->socket,handle);
    return(0);
    }
  else nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: %s - %s",tls->uriMap[tls->uriIndex].uri,strerror(errno));
  if(rc==-1 && errno==EINPROGRESS){
    nbLogMsg(context,0,'I',"nbTlsConnectNonBlockingAndSchedule: in progress - %s",tls->uriMap[tls->uriIndex]);
    nbListenerAddWrite(context,tls->socket,handle,handler); 
    return(tls->socket);
    } 
  nbLogMsg(context,0,'T',"nbTlsConnectNonBlockingAndSchedule: connect failed - %s",strerror(errno));
  return(rc);
  }
