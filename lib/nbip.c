/*
* Copyright (C) 2005-2014 Ed Trettevik <eat@nodebrain.org>
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
* Name:     nbip.c
*
* Title:    NodeBrain Internet Protocol (IP) Layer API
*
* Function:
*
*   This file provides a set of functions intended to simplify IP
*   layer communications by abstracting to a reduced functionality
*   set and handling portability to multiple platforms.
*   
* Description:
*
*   We export this API for use in NodeBrain Skill Modules that use
*   TCP and/or UDP communication.  We expect to transition NodeBrain's
*   built-in communications support to use this API.  Currently 
*   nbchannel.c provides support for both the TCP and encryption
*   layers. Moving the TCP layer from nbchannel.c to this file will
*   provide cleaner modularization. 
*
*   See nbip.h for a list of functions.
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2005-03-26 Ed Trettevik - original prototype version 0.6.2
* 2008-03-25 eat 0.7.0  Transitioning nbchannel routines without encryption support to here
* 2008-11-11 eat 0.7.3  Replaced a couple exit calls with return(-1)
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-01-31 dtl 0.8.6  Checker updates
* 2012-05-30 eat 0.8.10 Fixed nbIpGetDatagram - wasn't portable
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-27 eat 0.8.13 Checker updates
* 2012-12-31 eat 0.8.13 Checker updates
* 2013-01-11 eat 0.8.13 Checker updates
*=====================================================================
*/
#include <nb/nbi.h>
#if !defined(WIN32)
#include <sys/un.h>
#endif

/*
*  Look up the hostname for an ip address
*/
char *nbIpGetName(unsigned int ipaddr,char *name,size_t len){
  struct hostent *host;
  char addr[16];
  unsigned char *ipad=(unsigned char *)&ipaddr;

  *name=0;
  sprintf(addr,"%u.%u.%u.%u",*ipad,*(ipad+1),*(ipad+2),*(ipad+3));
  if(len<8) outMsg(0,'L',"nbIpGetName: hostname buffer must be at least 8 bytes - len=",len);
  else if((host=gethostbyaddr((char *)&ipaddr,sizeof(ipaddr),AF_INET))==NULL)
    outMsg(0,'E',"unable to get host name");
  else if(strlen(host->h_name)<len) strcpy(name,host->h_name); // 2012-10-17 eat - checker updates here  // 2012-12-31 eat - VID 5455-0.8.13-1 FP
  else{
    strncpy(name,host->h_name,len-1);
    *(name+len-1)=0;
    }
  return(name);
  }

int nbIpGetUdpClientSocket(unsigned short clientPort,char *addr,unsigned short port){
  int client_socket;
  struct sockaddr_in clientAddr,serverAddr;
  int domain=AF_INET;
  char ipaddr[512];
  char *delim;
  size_t len; // 2013-01-11 eat - VID 6875,6409,6405-0.8.13-2  - changed from int to size_t
#if !defined(WIN32)
  struct sockaddr_un un_addr;

  memset(&clientAddr,0,sizeof(clientAddr));  // 2012-12-15 eat - CID 751680
  memset(&serverAddr,0,sizeof(serverAddr));
  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(un_addr.sun_path)){
      outMsg(0,'E',"nbIpGetUdpClientSocket: Local domain socket path too long - %s",addr);
      return(-1);
      }
    fprintf(stderr,"domain set to AF_UNIX for %s\n",addr);
    }
#endif
  if((delim=strchr(addr,':'))!=NULL){
    len=delim-addr;
    if(len>=sizeof(ipaddr)){
      fprintf(stderr,"nbIpGetUpdClientSocket: Host portion of address is too long\n");
      return(-1);
      }
    strncpy(ipaddr,addr,len); 
    *(ipaddr+len)=0;
    addr=ipaddr;
    delim++;
    port=atoi(delim);
    }
  if((client_socket=socket(domain,SOCK_DGRAM,0))<0){
    fprintf(stderr,"nbIpGetUdpClientSocket: Unable to open UDP socket for sending datagrams to %s\n",addr);
    return(-1);
    }
  fprintf(stderr,"SOCK_DGRAM client socket=%d\n",client_socket);
  if(domain==AF_INET){
    clientAddr.sin_family=AF_INET;
    clientAddr.sin_addr.s_addr=inet_addr("0.0.0.0");
    clientAddr.sin_port=htons(clientPort);
    if(bind(client_socket,(struct sockaddr *)&clientAddr,sizeof(clientAddr))<0){
      perror("nbIpGetUdpClientSocket: Bind to UDP socket failed");
      close(client_socket);
      return(-1);
      }
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=inet_addr(addr);
    serverAddr.sin_port=htons(port);
    if(connect(client_socket,(struct sockaddr *)&serverAddr,sizeof(serverAddr))<0){
      perror("nbIpGetUdpClientSocket: Connect to UDP socket failed");
      close(client_socket);
      return(-1);
      }
    else fprintf(stderr,"nbIpGetUdpClientSocket: Connect to udp:%s:%d successful\n",addr,port);
    }
  else{
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,addr);
    fprintf(stderr,"un_addr.sun_path=%s\n",un_addr.sun_path);
    if(connect(client_socket,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      fprintf(stderr,"nbIpGetUdpClientSocket: Connect to local domain UDP socket failed - %s\n",strerror(errno));
      close(client_socket);
      return(-1);
      }
    }
  return(client_socket);
  }

int nbIpPutUdpClient(int socket,char *text){
  int rc;
  rc=send(socket,text,strlen(text)+1,0);
  if(rc<0) fprintf(stderr,"nbIpPutUdpClient: send failed - %s\n",strerror(errno));
  return(rc);
  }

/*
*  Get a server_socket to listen on
*/
unsigned int nbIpGetUdpServerSocket(NB_Cell *context,char *addr,unsigned short port){
  int server_socket,sockopt_enable=1;
  struct sockaddr_in in_addr;
  int domain=AF_INET;
  char ipaddr[512];
  char *delim;
  size_t len;  // 2013-01-11 eat VID 6244,6876,6890-0.8.13-2
#if !defined(WIN32)
  struct sockaddr_un un_addr;

  memset(&in_addr,0,sizeof(in_addr));  // 2012-12-16 eat - CID 751682
  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(un_addr.sun_path)){
      outMsg(0,'E',"nbIpGetUdpServerSocket: Local domain socket path too long - %s",addr);
      return(-1);
      }
    }
#endif
  if((delim=strchr(addr,':'))!=NULL){
    len=delim-addr;
    if(len>=sizeof(ipaddr)){
      fprintf(stderr,"nbIpGetUpdClientSocket: Host portion of address is too long\n");
      return(-1);
      }
    strncpy(ipaddr,addr,len); 
    *(ipaddr+len)=0;
    addr=ipaddr;
    delim++;
    port=atoi(delim);
    }
#if defined windows
  if((server_socket=socket(AF_INET,SOCK_DGRAM,0))==INVALID_SOCKET){
    outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to create socket. errno=%d",WSAGetLastError());
    return(server_socket);
    }
  // Prevent inheritance by child processes
  if(!SetHandleInformation((HANDLE)server_socket,HANDLE_FLAG_INHERIT,0)){
    outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to turn off socket inherit flag");
    close(server_socket);
    return(INVALID_SOCKET);
    }
#else
  if((server_socket=socket(domain,SOCK_DGRAM,0))<0){
    outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to create socket. errno=%d",errno);
    return(server_socket);
    }
#endif
  /* make sure we can reuse sockets when we restart */
  if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&sockopt_enable,sizeof(sockopt_enable))<0){
    outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to set socket option errno=%d",errno);
#if defined windows
    closesocket(server_socket);
#else
    close(server_socket);
#endif
    return(-1);
    }
  
//  server.sin_family = AF_INET;
//  server.sin_port = htons(port);
//  if(*addr==0) server.sin_addr.s_addr=INADDR_ANY;
//  else server.sin_addr.s_addr=inet_addr(addr);
//  if (bind(server_socket,(struct sockaddr *)&server,sizeof(server)) < 0) {
//    outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to bind socket. errno=%d",errno);
//#if defined windows
//    closesocket(server_socket);
//#else
//    close(server_socket);
//#endif
//    return(-1);
//    }
  if(domain==AF_INET){
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
    if(*addr==0) in_addr.sin_addr.s_addr=INADDR_ANY;
    else in_addr.sin_addr.s_addr=inet_addr(addr);
    if(bind(server_socket,(struct sockaddr *)&in_addr,sizeof(in_addr)) < 0) {
      outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to bind to inet domain socket %d. errno=%d",port,errno);
      nbIpCloseSocket(server_socket);
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain sockets
    un_addr.sun_family=AF_UNIX;
    //strcpy(un_addr.sun_path,addr);
    snprintf(un_addr.sun_path,sizeof(un_addr.sun_path),"%s",addr); // 2012-12-31 eat - VID 4953-0.8.13-1 - truncating if necessary
    if(unlink(addr)){ // 2012-12-31 eat - VID 5130-0.8.13-1
      if(trace) outMsg(0,'W',"nbIpGetUdpServerSocket: Unable to unlink before bind - %s",strerror(errno));
      }
    if(bind(server_socket,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      outMsg(0,'E',"nbIpGetUdpServerSocket: Unable to bind to local domain socket %s. errno=%d",addr,errno);
      nbIpCloseSocket(server_socket);
      return(-1);
      }
    }
#endif
  return(server_socket);
  }

/*
*  Receive a datagram (UDP)
*/
int nbIpGetDatagram(NB_Cell *context,int socket,unsigned int *raddr,unsigned short *rport,unsigned char *buffer,size_t length){
  struct sockaddr_in client;
  socklen_t sockaddrlen;
  int len;

  sockaddrlen=sizeof(client);
  len=recvfrom(socket,buffer,length,0,(struct sockaddr *)&client,&sockaddrlen); // 2012-12-31 eat - VID 640-0.8.13-1 FP
  if(len<0){
    if(errno!=EINTR){
      outMsg(0,'E',"nbIpGetDatagram: recvfrom failed. errno=%d",errno);
      exit(NB_EXITCODE_FAIL);
      }
    return(len);
    }
  //memcpy(raddr,&client.sin_addr,sizeof(raddr));  // 2012-05-30 eat - size corrected
  memcpy(raddr,&client.sin_addr,sizeof(int));  
  //*rport=client.sin_port; // 2012-05-30 eat - in network byte order
  *rport=ntohs(client.sin_port);
  return(len);
  }

char *nbIpGetAddrString(char *addr,unsigned int address){
  unsigned char *ipaddress=(unsigned char *)&address;
  sprintf(addr,"%3.3u.%3.3u.%3.3u.%3.3u",*ipaddress,*(ipaddress+1),*(ipaddress+2),*(ipaddress+3));
  return(addr);
  }

/*
*  Look up the ipaddr of a host
*/
char *nbIpGetAddrByName(char *hostname){
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


int nbIpGetSocketIpAddrString(int socket,char *ipaddr){
  struct sockaddr_in addr;
  socklen_t sockaddrlen;
  sockaddrlen=sizeof(addr);
  if(getsockname(socket,(struct sockaddr *)&addr,&sockaddrlen)<0){
    outMsg(0,'E',"Unable to get socket name.");
    return(-1);
    }
  sprintf(ipaddr,"%s",inet_ntoa(addr.sin_addr));
  return(0);
  }

int nbIpGetSocketAddrString(int socket,char *ipaddr){
  struct sockaddr_in addr;
  socklen_t sockaddrlen;
  sockaddrlen=sizeof(addr);
  if(getsockname(socket,(struct sockaddr *)&addr,&sockaddrlen)<0){
    outMsg(0,'E',"Unable to get socket name.");
    return(-1);
    }
  sprintf(ipaddr,"%s:%d",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));
  return(0);
  }

int nbIpGetTcpSocketPair(int *connectSocket,int *acceptSocket){
#if defined(WIN32)
  int listener=-1;
  int connector=-1;
  int acceptor=-1;
  size_t sockaddrlen;
  struct sockaddr_in listenAddr,connectAddr;
  if((listener=socket(AF_INET,SOCK_STREAM,0))==SOCKET_ERROR){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get listener socket");
    return(-1);
    }
  memset(&listenAddr,0,sizeof(listenAddr));
  listenAddr.sin_family=AF_INET;
  listenAddr.sin_addr.S_un.S_addr=htonl(INADDR_LOOPBACK);
  listenAddr.sin_port=0;
  if(bind(listener,(struct sockaddr *)&listenAddr,sizeof(listenAddr))==SOCKET_ERROR){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to bind listener socket");
    return(-1);
    }
  if(listen(listener,5)!=0){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to listen");
    closesocket(listener);
    return(-1);
    }
  sockaddrlen=sizeof(connectAddr);
  if(getsockname(listener,(struct sockaddr *)&connectAddr,&sockaddrlen)==-1){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get connect address");
    closesocket(listener);
    return(-1);
    }
  if((connector=socket(AF_INET,SOCK_STREAM,0))==SOCKET_ERROR){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get connector socket");
    closesocket(listener);
    return(-1);
    }
  if(connect(connector,(struct sockaddr *)&connectAddr,sizeof(connectAddr))==SOCKET_ERROR){
    closesocket(listener);
    closesocket(connector);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to connect to listener");
    return(-1);
    }
  sockaddrlen=sizeof(listenAddr);
  acceptor=accept(listener,(struct sockaddr *)&listenAddr,&sockaddrlen);
  closesocket(listener);
  if(acceptSocket<0){
    closesocket(connector);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to accept connection");
    return(-1);
    }
  if(sockaddrlen!=sizeof(listenAddr)){
    closesocket(acceptor);
    closesocket(connector);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Address size confusion");
    return(-1);
    }
  // is this necessary --- we already have the address don't we?
  if(getsockname(connector,(struct sockaddr *)&connectAddr,&sockaddrlen)==SOCKET_ERROR){
    closesocket(acceptor);
    closesocket(connector);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get connector address");
    return(-1);
    }  
  if(sockaddrlen!=sizeof(connectAddr)
      || listenAddr.sin_family != connectAddr.sin_family
      || listenAddr.sin_addr.s_addr != connectAddr.sin_addr.s_addr
      || listenAddr.sin_port != connectAddr.sin_port){
    closesocket(acceptor);
    closesocket(connector);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Addresses don't match");
    return(-1);
    }
  *connectSocket=connector;
  *acceptSocket=acceptor;
  return(0);
  }
#else
  int sock[2];
  if(socketpair(AF_UNIX,SOCK_STREAM,0,sock)<0){
    outMsg(0,'L',"nbIpGetTcpSocketPair: socketpair failed - errno=%d",errno);
    return(-1);
    }
  *connectSocket=sock[0];
  *acceptSocket=sock[1];
  return(0);
  }
#endif

// Get a UDP socket bound to a system generated port number
int nbIpGetUdpSocket(int *sock,struct sockaddr_in *socketaddr){
  memset(socketaddr,0,sizeof(struct sockaddr_in));
  if((*sock=socket(AF_INET,SOCK_DGRAM,0))<0) return(-1);
  socketaddr->sin_family = AF_INET;
  socketaddr->sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  socketaddr->sin_port=0;
  //if(bind(*sock,(struct sockaddr *)socketaddr,sizeof(socketaddr))<0) return(-1);  // 2012-12-16 eat - CID 751515
  if(bind(*sock,(struct sockaddr *)socketaddr,sizeof(struct sockaddr_in))<0) return(-1);
  return 0;
  }

// This function has not been tested and probably needs work on Windows
int nbIpGetUdpSocketPair(int *socket1,int *socket2){
#if defined(WIN32)
  struct sockaddr_in socketaddr1,socketaddr2;
  if(nbIpGetUdpSocket(socket1,&socketaddr1)){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get TCP socket 1");
    return(-1);
    }
  if(nbIpGetUdpSocket(socket2,&socketaddr2)){
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to get TCP socket 2");
    closesocket(*socket1);
    return(-1);
    }
  if(connect(*socket2,(struct sockaddr *)&socketaddr1,sizeof(socketaddr1))==SOCKET_ERROR){
    closesocket(*socket1);
    closesocket(*socket2);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to connect socket 2");
    return(-1);
    }
  if(connect(*socket1,(struct sockaddr *)&socketaddr2,sizeof(socketaddr2))==SOCKET_ERROR){
    closesocket(*socket1);
    closesocket(*socket2);
    outMsg(0,'L',"nbIpGetTcpSocketPair: Unable to connect socket 2");
    return(-1);
    }
  return(0); 
  }
#else
  int sock[2];
  if(socketpair(AF_UNIX,SOCK_DGRAM,0,sock)<0){
    outMsg(0,'L',"nbIpGetTcpSocketPair: socketpair failed - errno=%d",errno);
    return(-1);
    }
  *socket1=sock[0];
  *socket2=sock[1];
  return(0);
  }
#endif

/*========================================================================================
* The following routines are replacements for the chchannel.c routines with the support
* for NBP encryption removed.
*/

/*
*  Allocate channel descriptor (used by client before call to chopen)
*/
struct IP_CHANNEL *nbIpAlloc(void){
  NB_IpChannel *channel;

  channel=(NB_IpChannel *)nbAlloc(sizeof(NB_IpChannel));
  channel->socket=0;
  *(channel->ipaddr)=0;
  *(channel->unaddr)=0;
  return(channel);
  }

/*
*  Get a server_socket to listen on
*     NOTE: replace all calls to nbIpListen with this
*/
int nbIpListen(char *addr,unsigned short port){
  int server_socket,sockopt_enable=1;
  struct sockaddr_in in_addr;
  int domain=AF_INET;
#if !defined(WIN32)
  struct sockaddr_un un_addr;

  memset(&in_addr,0,sizeof(in_addr));  // 2012-12-16 eat - CID 751683
  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(un_addr.sun_path)){
      outMsg(0,'E',"nbIpListen: Local domain socket path too long - %s",addr);
      return(-1);
      }
    }
#endif
#if defined(WIN32)
//  HANDLE dup,hProcess = GetCurrentProcess();
  if((server_socket=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET) {
    outMsg(0,'E',"nbIpListen() Unable to create socket. errno=%d",WSAGetLastError());
    return(server_socket);
    }
  // Prevent inheritance by child processes
  if(!SetHandleInformation((HANDLE)server_socket,HANDLE_FLAG_INHERIT,0)){
    outMsg(0,'E',"nbIpListen() Unable to turn off socket inherit flag");
          closesocket(server_socket);
          return(INVALID_SOCKET);
          }
  // Another way to prevent inheritance
//  if(DuplicateHandle(hProcess,(HANDLE)server_socket,hProcess,&dup,0,FALSE,DUPLICATE_SAME_ACCESS)) {
//    closesocket(server_socket);
//    server_socket=(SOCKET)dup;
//    }
//  else{ outMsg(0,'E',"nbIpListen() unable to duplicate server socket"); }
#else
  if ((server_socket=socket(domain,SOCK_STREAM,0)) < 0) {
    outMsg(0,'E',"nbIpListen: Unable to create socket - %s",strerror(errno));
    return(server_socket);
    }
  if(fcntl(server_socket,F_SETFD,FD_CLOEXEC)){
    outMsg(0,'E',"nbIpListen: Unable to fcntl close on exec - %s",strerror(errno));
    nbIpCloseSocket(server_socket);
    return(-1);
    }
#endif
  /* make sure we can reuse sockets when we restart */
  /* we say the option value is char for windows */
  if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&sockopt_enable,sizeof(sockopt_enable))<0){
    outMsg(0,'E',"nbIpListen: Unable to set socket option errno=%d",errno);
    nbIpCloseSocket(server_socket);
    return(-1);
    }
  if(domain==AF_INET){
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
    if(*addr==0) in_addr.sin_addr.s_addr=INADDR_ANY;
    else in_addr.sin_addr.s_addr=inet_addr(addr);
    if (bind(server_socket,(struct sockaddr *)&in_addr,sizeof(in_addr)) < 0) {
      outMsg(0,'E',"nbIpListen: Unable to bind to inet domain socket on port %d - %s",port,strerror(errno));
      nbIpCloseSocket(server_socket);
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain sockets
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,addr);
    if(unlink(addr)){ // 2012-12-31 eat - VID 738-0.8.13-1
      outMsg(0,'W',"nbIpListen: Unable to unlink before bind - %s",strerror(errno));
      }
    if(bind(server_socket,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      outMsg(0,'E',"nbIpListen: Unable to bind to local domain socket %s - %s",addr,strerror(errno));
      nbIpCloseSocket(server_socket);
      return(-1);
      }
    }
#endif
  if (listen(server_socket,5) != 0) {
    outMsg(0,'E',"nbIpListen: Unable to listen. errno=%d",errno);
    nbIpCloseSocket(server_socket);
    return(-2);
    }
  return(server_socket);
  }

/*
*  Close a server_socket
*
*  This may seem silly, but it prevents skill modules from having
*  to link with the socket library just to get closesocket on windows.
*
*  NOTE: replace all calls to chclosesocket with this
*/
void nbIpCloseSocket(int server_socket){
#if defined(WIN32)
  closesocket(server_socket);
#else
  close(server_socket);
#endif
  }

/*
*  Accept a channel connection
*/
int nbIpAccept(NB_IpChannel *channel,int server_socket){
  struct sockaddr_in client;
#if defined(WIN32) || defined(mpe) || defined(SOLARIS) || defined(MACOS)
  size_t sockaddrlen;
#else
  socklen_t sockaddrlen;
#endif
  sockaddrlen=sizeof(client);
#if defined(MACOS)
  channel->socket=accept(server_socket,(struct sockaddr *)&client,(int *)&sockaddrlen);
#else
  channel->socket=accept(server_socket,(struct sockaddr *)&client,&sockaddrlen);
#endif
  if(channel->socket<0){
    if(errno!=EINTR) outMsg(0,'E',"nbIpAccept: accept failed - ",strerror(errno));
    return(channel->socket);
    }
#if !defined(WIN32)
  if(fcntl(channel->socket,F_SETFD,FD_CLOEXEC)!=0){ // 2012-12-27 eat 0.8.13 - CID 751518
    outMsg(0,'E',"nbIpAccept: fcntl failed - %s",strerror(errno));
    close(channel->socket);
    return(-1); 
    }
#endif
#if defined(mpe)
  strcpy(channel->ipaddr,(char *)inet_ntoa(client.sin_addr));
#else
  strncpy(channel->ipaddr,inet_ntoa(client.sin_addr),sizeof(channel->ipaddr)-1); // 2012-12-16 eat - CID 751636
  *(channel->ipaddr+sizeof(channel->ipaddr)-1)=0;
#endif
  channel->port=ntohs(client.sin_port);
  return(0);
  }

/*
*  Send a data record
*
*      Plain  text:   len,plaintext
*/
int nbIpPut(NB_IpChannel *channel,char *buffer,int len){
  int sent;

  if(len>NB_BUFSIZE){
    outMsg(0,'E',"chput: Length %u too large.",len);
    return(-1);
    }
  if(len==0){
    outMsg(0,'L',"chput: Length %u too short - use chstop",len);
    }
  if (len>0) memcpy(channel->buffer,buffer,len); //dtl: added len check 
  channel->len=htons((unsigned short)len);
  len+=2;
  sent=send(channel->socket,(unsigned char *)&(channel->len),len,0);
  while(sent==-1 && errno==EINTR){
    sent=send(channel->socket,(unsigned char *)&(channel->len),len,0);
    }
  return(sent);
  }

/* chputmsg()
*
*  This is an experimentatl channel routine for sending messages to a console. It will be
*  used by nb_mod_console.c for prototyping the console.  Currently only the Java code of
*  the console knows how to recognize messages and distinguish from conversation packets.
*  If this approach works out, we need to move this functionality to chput and chget.  They
*  just need to recognize a bit in the length value.
*
*     x8000 Off - Conversation
*           On  - Message
*/
int nbIpPutMsg(NB_IpChannel *channel,char *buffer,size_t len){ // 2012-12-31 eat - VID 4735,5047,5132-0.8.13-1 - len from int to size_t
  int sent;
  unsigned char packet[NB_BUFSIZE];

  if(len>4094 || len>sizeof(packet)-2){
    outMsg(0,'L',"nbIpPutMsg: Length %u too large.",len);
    return(-1);
    }
  *(packet)=(unsigned char)((len>>8)|0x80);  // 2013-01-11 eat VID 7048-0.8.13-2
  *(packet+1)=(unsigned char)(len%256);      // 2013-01-11 eat VID 6245-0.8.13-2
  memcpy(packet+2,buffer,len);  // 2012-12-31 eat - VID 763-0.8.13-1 FP - unless it goes away from type change above
  outMsg(0,'T',"nbIpPutMsg: len=%u ",len);
  len+=2;
  sent=send(channel->socket,packet,len,0);
  while(sent==-1 && errno==EINTR){
    sent=send(channel->socket,packet,len,0);
    }
  return(sent);
  }


/*
*  Send a stop record (null)
*/
int nbIpStop(NB_IpChannel *channel){
  int sent;
  channel->len=0;
  sent=send(channel->socket,(unsigned char *)&(channel->len),2,0);
  while(sent==-1 && errno==EINTR){
    sent=send(channel->socket,(unsigned char *)&(channel->len),2,0);
    }
  return(sent);
  }

/*
*  Receive a data or stop record
*/
int nbIpGet(NB_IpChannel *channel,char *buffer){
  static unsigned short len,expect;
  static char *cursor;
  int i;

  i=recv(channel->socket,(unsigned char *)&(channel->len),2,0);
  while(i==-1 && errno==EINTR){
    i=recv(channel->socket,(unsigned char *)&(channel->len),2,0);
    }
  if(i<2){
#if defined(WIN32)
    outMsg(0,'E',"chget: Expected length field not received. (%d), errno=%d",i,WSAGetLastError());
#else
    outMsg(0,'E',"chget: Expected length field not received. (%d), errno=%d",i,errno);
#endif
    return(-2);
    }
  if((len=ntohs(channel->len))==0) return(0); /* null */
  if(len>NB_BUFSIZE){
    outMsg(0,'E',"chget: Invalid record encountered. Length %d too large.",len);
    return(-2);
    }
  cursor=(char *)(channel->buffer);
  expect=len;
  i=recv(channel->socket,(void *)cursor,expect,0);
  while(i==-1 && errno==EINTR){
    i=recv(channel->socket,(void *)cursor,expect,0);
    }
  while(i>0 && i<expect){
    cursor+=i;
    expect-=i;
    i=recv(channel->socket,(void *)cursor,expect,0);
    while(i==-1 && errno==EINTR){
      i=recv(channel->socket,(void *)cursor,expect,0);
      }
    }
  if(i!=expect){
    outMsg(0,'E',"chget: Invalid record encountered. Expecting %d more bytes. Received %d.  errno=%d",expect,i,errno);
    return(-3);
    }
  memcpy(buffer,channel->buffer,len); // 2013-01-13 eat - restored
  return(len);
  }
/*
*  Close channel
*/
int nbIpClose(NB_IpChannel *channel){
#if defined(WIN32)
  return(closesocket(channel->socket));
#else
  return(close(channel->socket));
#endif
  }

/*
*  Free channel descriptor
*/
int nbIpFree(NB_IpChannel *channel){
  nbFree(channel,sizeof(NB_IpChannel));
  return(0);
  }

int nbIpTcpConnect(char *addr,unsigned short port){
  int tcpSocket;
  struct sockaddr_in in_addr;
  struct in_addr inaddr;
  int domain=AF_INET;
#if !defined(WIN32)
  struct sockaddr_un un_addr;

  memset(&in_addr,0,sizeof(in_addr));  // 2012-12-16 eat - CID 751684
  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(un_addr.sun_path)){
      nbLogMsgI(0,'E',"chopen: Local domain socket path too long - %s",addr);
      return(-1);
      }
    }
#endif
#if defined(WIN32)
  if((tcpSocket=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET){
    return(tcpSocket);
    }
#else
  if((tcpSocket=socket(domain,SOCK_STREAM,0))<0){
    return(tcpSocket);
    }
#endif
  if(domain==AF_INET){
    inaddr.s_addr=inet_addr(addr);
    in_addr.sin_family = AF_INET;
    in_addr.sin_addr = inaddr;
    in_addr.sin_port = htons(port);
    if((connect(tcpSocket,(struct sockaddr *)&in_addr,sizeof(in_addr))) < 0){
      nbIpCloseSocket(tcpSocket);
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain socket
    inaddr.s_addr=inet_addr(addr);
    un_addr.sun_family = AF_UNIX;
    strcpy(un_addr.sun_path,addr);
    if ((connect(tcpSocket,(struct sockaddr *)&un_addr,sizeof(un_addr))) < 0){
      nbIpCloseSocket(tcpSocket);
      return(-1);
      }
    }
#endif
  return(tcpSocket);
  }

