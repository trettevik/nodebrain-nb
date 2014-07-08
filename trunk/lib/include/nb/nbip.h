/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbip.h
*
* Title:    Header for Internet Protocol (IP) Routines (prototype)
*
* Function:
*
*   This header supports nbip.c functions for internal use.  See nbapi.h for
*   API functions.  See nbchannel.c for functionality that may transition to
*   nbip.c as we separate the IP layer from our encryption layer.
*
* Synopsis:
*
*   #include "nbip.h"
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005-03-26 Ed Trettevik (original prototype 0.6.2)
* 2005-04-08 eat 0.6.2  Moved API functions definitions to nbapi.h
* 2008-03-24 eat 0.7.0  Started IP_CHANNEL structure as alternative to NBP CHANNEL
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-12-25 eat 0.8.13 Corrected buffer data type
*=============================================================================
*/
#ifndef _NB_IP_H_
#define _NB_IP_H_

#include <nb/nbcell.h>

typedef struct IP_CHANNEL{
  int socket;                /* socket number or handle */
  char ipaddr[16];           /* ip address of peer */
  char unaddr[256];          // local (unix) domain socket path
  unsigned short port;       /* port to communicate with */
  unsigned short len;        /* Buffer length in bytes */
  //unsigned int buffer[NB_BUFSIZE]; /* buffer - must follow len */ // 2012-12-25 eat - AST 57
  unsigned char buffer[NB_BUFSIZE]; /* buffer - must follow len */
  } NB_IpChannel;


#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGetTcpSocketPair(int *socket1,int *socket2);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbIpGetName(unsigned int ipaddr,char *name,size_t len);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern unsigned int nbIpGetUdpServerSocket(nbCELL context,char *interfaceAddr,unsigned short port);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGetDatagram(nbCELL context,int socket,unsigned int *raddr,unsigned short *rport,unsigned char *buffer,size_t length);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbIpGetAddrString(char *addr,unsigned int address);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbIpGetAddrByName(char *hostname);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGetSocketIpAddrString(int socket,char *ipaddr);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGetSocketAddrString(int socket,char *ipaddr);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct IP_CHANNEL *nbIpAlloc(void);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpListen(char *addr,unsigned short port);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbIpCloseSocket(int server_socket);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpAccept(NB_IpChannel *channel,int server_socket);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpPut(NB_IpChannel *channel,char *buffer,int len);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpPutMsg(NB_IpChannel *channel,char *buffer,size_t len);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpStop(NB_IpChannel *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGet(NB_IpChannel *channel,char *buffer);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpClose(NB_IpChannel *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpFree(NB_IpChannel *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpGetUdpClientSocket(unsigned short clientPort,char *address,unsigned short port);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpPutUdpClient(int socket,char *text);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbIpTcpConnect(char *addr,unsigned short port);

#endif
