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
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbchannel.c 
*
* Title:    Channel Protocol Routines (prototype)
*
* Function:
*
*   This file provides routines that implement the NodeBrain Channel
*   protocol.  This is a low level application layer of TCP/IP socket
*   communication that isolates higher level protocols from platform
*   variations, and provides data encryption.
*
* Synopsis:
*
*   #include "nb.h"
*
*   struct CHANNEL *challoc();
*   int chlisten(char *address,unsigned short port);  // returns server_socket 
*   int chaccept(struct CHANNEL *channel,int server_socket); // returns open channel
*   int chgetaddr(char *ipaddr,char *hostname);  // get string ipaddr using gethostbyname  
*   int chopen(struct CHANNEL *channel, char *ipaddr, unsigned short port);
*   int chput(struct CHANNEL *channel, char *buffer, unsigned short len);
*   int chstop(struct CHANNEL *channel);
*   int chget(struct CHANNEL *channel, char *buffer);
*   int chclose(struct CHANNEL *channel); 
*   int chfree(struct CHANNEL *channel);
*   int close(int server_socket);
*
*   char *chgetname(char *ipaddr);  // get hostname using gethostbyaddr
*
*   void chkey(channel,enKey,deKey,enCipher,deCipher) // assign secret encryption keys
*
* Description
*
*   These routines are used to implement a NodeBrain server or client.  An outline
*   of a server without error checking is shown here.
*
*        struct CHANNEL *channel;
*        int server_socket;
*        channel=challoc();
*        server_socket=chlisten(address,port);
*        while(...listening...){
*          struct CHANNEL *channel;
*          chaccept(channel,server_socket);
*          while(...chatting...){
*            chget/chput/chstop(channel,...);
*            }
*          [chstop(channel);]
*          chclose(channel);
*          }
*        chfree(channel);
*        close(server_socket);
*
*   An outline of a version X client without error checking follows.
*
*        struct CHANNEL *channel;
*        channel=challoc();
*        while(...have input...){
*          ...decide who to connect to...
*          chopen(channel,ipaddr,port);
*          chput(channel,buffer,len);
*          chstop(channel);
*          while((len=chget(channel,buffer))>0){
*            ...process response...
*            }
*          chclose(channel);
*          }
*        chfree(channel);
*
*   An outline of a version Y client without error checking follows.
*
*        struct CHANNEL *channel;
*        channel=challoc();
*        while(...have input...){
*          ...decide who to connect to...
*          chopen(channel,ipaddr,port);
*          len=0;
*          while(...have batched input for given brain...){
*            chput(channel,buffer,len);
*            while((len=chget(channel,buffer))>0){
*              ...process response...
*              }
*            }
*          if(len==0) chstop(channel);
*          chclose(channel);
*          }
*        chfree(channel);
*   
* Exit Codes:
*
*   0 - Successful completion
*   1 - Error (see message)
*
*   <0 - Error (chput, chget).
* 
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 1999/03/04 Ed Trettevik (original prototype version)
* 2000/10/09 eat - version 0.2.2
*             1) included conditional windows (winsock) code
* 2000/10/13 eat - version 0.2.2
*             1) chkey(channel,enKey,deKey,cipher) added to assign secret encryption
*                keys.  This is not included in challoc because initial communication 
*                may be required for exchange of secret keys.  This would be done
*                under the protection of public/private keys.
*             2) added secret key encryption to chput and chget
* 2000/10/16 eat - version 0.2.2
*             1) convert data to/from network byte order in chput and chget 
* 2001/02/11 eat - version 0.2.4
*             1) Switched to separate CBC keys for get and put.
*             2) Call skeRandCipher to change the CBC key instead of calling the
*                skeCipher.  This was necessary because the peers no longer have
*                the same Rijndael key (one encrypts and the other decrypts).
*                The skeRandCipher is a primative pseudo-random number generator
*                that will work the same for both peers. 
* 2001/04/07 eat - version 0.2.5
*             1) Included setsockopt() call to force reuse of sockets.  This
*                resolves restart failure problem, primarily on solaris systems.           
* 2001/05/23 eat - version 0.2.6
*             1) Modified chget to continue recv'ing until it gets the expected
*                number of bytes.
*             2) Note: We need to include buffer overflow protection.
* 2001/10/06 eat - version 0.2.9
*             1) Check included in chget and chput to prevent buffer overflow.
*                Callers must provide NB_BUFSIZE byte buffer.
* 2002/02/19 eat - version 0.3.0
*             1) Oops, the channel structure had a 1k buffer.  Changed
*                it to 4k.
*
* 2003/03/03 eat 0.5.1  Conditional compile for Max OS X [ see defined(MACOS) ]
* 2003/07/19 eat 0.5.4  Set server socket to uninheritable for Windows.
* 2006/05/25 eat 0.6.6  chlisten() and chopen() modified to support local (unix) domain sockets
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-31 dtl 0.8.7  Checker updates
* 2012-02-09 eat 0.8.7  Reviewed Checker
* 2012-06-16 eat 0.8.10 Replaced rand with random
* 2012-08-31 dtl 0.8.12 Checker updates
* 2012-10-31 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
*=============================================================================
*/
#include "nbi.h"
#include "nbske.h"
#include "nbchannel.h"
#if !defined(WIN32)
#include <sys/un.h>
#endif
  
/*
*  Allocate channel descriptor (used by client before call to chopen) 
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct CHANNEL *challoc(void){
  struct CHANNEL *channel;
   
  channel=nbAlloc(sizeof(struct CHANNEL));
  channel->enKey.mode=0;
  channel->deKey.mode=0;
  channel->socket=0;
  *(channel->ipaddr)=0;
  *(channel->unaddr)=0;
  return(channel);
  }
  
/*
*  Assign secret key encryption keys
*    The AES encryption is optional.
*/

void chkey(struct CHANNEL *channel,skeKEY *enKey,skeKEY *deKey,unsigned int enCipher[4],unsigned int deCipher[4]){
  memcpy(&(channel->enKey),enKey,sizeof(skeKEY));
  memcpy(&(channel->deKey),deKey,sizeof(skeKEY));
  memcpy(channel->enCipher,enCipher,16);
  memcpy(channel->deCipher,deCipher,16);  
  }   
  
/*
*  Get a server_socket to listen on
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chlisten(char *addr,unsigned short port){
  int server_socket,sockopt_enable=1;
  struct sockaddr_in in_addr;
  int domain=AF_INET;
#if !defined(WIN32)
  struct sockaddr_un un_addr;

  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(un_addr.sun_path)){
      nbLogMsgI(0,'E',"chlisten: Local domain socket path too long - %s",addr);
      return(-1);
      }
    }
#endif
#if defined(WIN32)
//  HANDLE dup,hProcess = GetCurrentProcess();
  if((server_socket=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET) {
    nbLogMsgI(0,'E',"chlisten() Unable to create socket. errno=%d",WSAGetLastError());
    return(server_socket);
    }
  // Prevent inheritance by child processes 
  if(!SetHandleInformation((HANDLE)server_socket,HANDLE_FLAG_INHERIT,0)){
    nbLogMsgI(0,'E',"chlisten() Unable to turn off socket inherit flag");
	  closesocket(server_socket);
	  return(INVALID_SOCKET);
	  }
  // Another way to prevent inheritance
//  if(DuplicateHandle(hProcess,(HANDLE)server_socket,hProcess,&dup,0,FALSE,DUPLICATE_SAME_ACCESS)) {
//    closesocket(server_socket);
//    server_socket=(SOCKET)dup;
//    }
//  else{ nbLogMsg(NB_LOG_CONEXT,0,'E',"chlisten() unable to duplicate server socket"); }
#else    
  if ((server_socket=socket(domain,SOCK_STREAM,0)) < 0) {
    nbLogMsgI(0,'E',"chlisten: Unable to create socket. errno=%d",errno);
    return(server_socket);
    }
  fcntl(server_socket,F_SETFD,FD_CLOEXEC);
#endif
  /* make sure we can reuse sockets when we restart */
  /* we say the option value is char for windows */
  if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&sockopt_enable,sizeof(sockopt_enable))<0){
    nbLogMsgI(0,'E',"chlisten: Unable to set socket option errno=%d",errno);
    chclosesocket(server_socket);
    return(-1);
    }    
  if(domain==AF_INET){
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
    if(*addr==0) in_addr.sin_addr.s_addr=INADDR_ANY;
    else in_addr.sin_addr.s_addr=inet_addr(addr);
    if (bind(server_socket,(struct sockaddr *)&in_addr,sizeof(in_addr)) < 0) {
      nbLogMsgI(0,'E',"chlisten: Unable to bind to inet domain socket %d. errno=%d",port,errno);
      chclosesocket(server_socket);
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain sockets
    un_addr.sun_family=AF_UNIX;
    strcpy(un_addr.sun_path,addr);
    //unlink(addr);
    if(unlink(addr));         //2012-08-31 dtl: checked unlink result
      //nbLogMsgI(0,'E',"chlisten: unlink() failed.");  // 2012-10-13 eat - this may be ok if not found, let bind complain if necessary
    if(bind(server_socket,(struct sockaddr *)&un_addr,sizeof(un_addr))<0){
      nbLogMsgI(0,'E',"chlisten: Unable to bind to local domain socket %s. errno=%d",addr,errno);
      chclosesocket(server_socket);
      return(-1);
      }
    }
#endif
  if (listen(server_socket,5) != 0) {
    nbLogMsgI(0,'E',"chlisten: Unable to listen. errno=%d",errno);
    chclosesocket(server_socket);
    return(-2);
    } 
  return(server_socket);  
  }

/* 
*  Close a server_socket
*
*  This may seem silly, but it prevents skill modules from having
*  to link with the socket library just to get closesocket on windows.    
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern void chclosesocket(int server_socket){
#if defined(WIN32)
  closesocket(server_socket);
#else    
  close(server_socket);
#endif
  }
  
/*
*  Accept a channel connection
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chaccept(struct CHANNEL *channel,int server_socket){
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
    if(errno!=EINTR) nbLogMsgI(0,'E',"chaccept: accept failed.");
    return(channel->socket);
    }
#if !defined(WIN32)
  fcntl(channel->socket,F_SETFD,FD_CLOEXEC);
#endif
#if defined(mpe)
  strcpy(channel->ipaddr,(char *)inet_ntoa(client.sin_addr));
#else
  strcpy(channel->ipaddr,inet_ntoa(client.sin_addr));
#endif
  channel->port=ntohs(client.sin_port);
  return(0);
  }   

/*
*  Look up the ipaddr of a host
*/
char *chgetaddr(char *hostname){
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
*  Look up the hostname for an ip address
*/
char *chgetname(char *ipaddr){
  struct hostent *host;
  u_int addr;
  
  if((int)(addr=inet_addr(ipaddr))==-1) return(NULL);
  if((host=gethostbyaddr((char *)&addr,sizeof(addr),AF_INET))==NULL) return(NULL);
  return(host->h_name);
  }
  
/*
*  Open a channel for communcations with another brain
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chopen(struct CHANNEL *channel,char *addr,unsigned short port){
  struct sockaddr_in in_addr;
  struct in_addr inaddr;
  int domain=AF_INET;
#if !defined(WIN32)
  struct sockaddr_un un_addr;
  
  if(*addr!=0 && (*addr<'0' || *addr>'9')){
    domain=AF_UNIX;
    if(strlen(addr)>sizeof(channel->unaddr) || strlen(addr)>sizeof(un_addr.sun_path)){
      nbLogMsgI(0,'E',"chopen: Local domain socket path too long - %s",addr);
      return(-1);
      }
    strcpy(channel->unaddr,addr);
    }
  else strcpy(channel->ipaddr,addr);
#else
  strcpy(channel->ipaddr,addr);
#endif
  channel->port=port;
#if defined(WIN32)
  if((channel->socket=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET) {
    //if(trace) nbLogMsgI(0,'E',"chopen: Unable to create socket. errno=%d",WSAGetLastError());
    return(channel->socket);
    }
#else      
  if ((channel->socket=socket(domain,SOCK_STREAM,0)) < 0) {
    //if(trace) nbLogMsgI(0,'E',"chopen: Unable to create socket. errno=$d",errno);
    return(channel->socket);
    }
#endif    
  if(domain==AF_INET){ 
    inaddr.s_addr=inet_addr(addr);     
    in_addr.sin_family = AF_INET;
    in_addr.sin_addr = inaddr;
    in_addr.sin_port = htons(port);
    // modify so connect attempt will timeout
    //fcntl(channel->socket,    set O_NONBLOCK
    //rc=connect(channel->socket,(struct sockaddr *)&in_addr,sizeof(in_addr));
    // set timeout interval
    //rc=select(...)
    if((connect(channel->socket,(struct sockaddr *)&in_addr,sizeof(in_addr))) < 0){
      //if(trace) nbLogMsgI(0,'E',"chopen: Unable to connect to inet domain socket %d - errno=%d",port,errno);
      chclosesocket(channel->socket);
      return(-1);
      }
    }
#if !defined(WIN32)
  else{  // handle local (unix) domain socket
    inaddr.s_addr=inet_addr(addr);
    un_addr.sun_family = AF_UNIX;
    strcpy(un_addr.sun_path,addr);
    if ((connect(channel->socket,(struct sockaddr *)&un_addr,sizeof(un_addr))) < 0){
      //if(trace) nbLogMsgI(0,'E',"chopen: Unable to connect to local domain socket %s - errno=%d",addr,errno);
      chclosesocket(channel->socket);
      return(-1);
      }
    }
#endif
  return(0);
  }  

/*
*  Send a data record - encrypted if key has been provided
*
*      Plain  text:   len,plaintext
*      Cipher text:   len,used,ciphertext    (last word is checksum)
*/  
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chput(struct CHANNEL *channel,char *buffer,size_t len){  // 2012-10-13 eat changed len from int to size_t
  static unsigned int checksum,i;
  int sent;

  if(len>NB_BUFSIZE-20){
    nbLogMsgI(0,'E',"chput: Length %u too large.",len);
    return(-1);
    }  
  else if(len==0){
    nbLogMsgI(0,'L',"chput: Length %u too short - use chstop",len);
    return(-1);
    }
  memcpy(channel->buffer,buffer,len);
  if(channel->enKey.mode){
    i=((len+5+15)&0xfffffff0)-len;   /* pad up to 16 byte boundary */ // 2012-10-13 eat - Note: 5 <= i <= 20 
    if(i>5) memset(((unsigned char *)channel->buffer)+len,random()&0xff,i-5);
    len+=i; // 2012-1013 eat - len>5 now because i>=5 and len was >0
    *((unsigned char *)channel->buffer+len-5)=i;  // 2012-10-13 eat - don't worry, len>5 and i<=20
    checksum=0;                                 /* initialize checksum */
    for(i=0;i<(unsigned short)(len/4-1);i++){
      channel->buffer[i]=ntohl(channel->buffer[i]);
      checksum+=channel->buffer[i]; /* compute checksum */
      }
    channel->buffer[i]=checksum;                         /* include checksum */
    /* skeCipher(channel->enCipher,1,channel->enCipher,channel->enKey);*/  /* encrypt cipher */
    skeRandCipher(channel->enCipher);
    skeCipher(channel->buffer,len/16,channel->enCipher,&(channel->enKey));
    for(i=0;i<(unsigned short)(len/4);i++) channel->buffer[i]=htonl(channel->buffer[i]);
    }
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
extern int chputmsg(struct CHANNEL *channel,char *buffer,size_t len){
  int sent;
  unsigned char packet[NB_BUFSIZE];

  if(len>NB_BUFSIZE-2){   //2012-01-31 dtl: added neg len test
    nbLogMsgI(0,'E',"chput: Length %u too large.",len);
    return(-1);
    }  
  *(packet)=(len>>8)|0x80;
  *(packet+1)=len&255;
  memcpy(packet+2,buffer,len); //dtl: len already tested
  nbLogMsgI(0,'T',"chputmsg() len=%u ",len);
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
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chstop(struct CHANNEL *channel){
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
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chget(struct CHANNEL *channel,char *buffer){
  static unsigned short len,expect;
  static unsigned int checksum;
  static char *cursor;
  int i;

  i=recv(channel->socket,(unsigned char *)&(channel->len),2,0);
  while(i==-1 && errno==EINTR){
    i=recv(channel->socket,(unsigned char *)&(channel->len),2,0);
    }
  if(i<2){
#if defined(WIN32)
    nbLogMsgI(0,'E',"chget: Expected length field not received. (%d), errno=%d",i,WSAGetLastError());
#else
    nbLogMsgI(0,'E',"chget: Expected length field not received. (%d), errno=%d",i,errno);
#endif
    return(-2);
    }
  if((len=ntohs(channel->len))==0) return(0); /* null */
  if(len>NB_BUFSIZE){
    nbLogMsgI(0,'E',"chget: Invalid record encountered. Length %d too large.",len);
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
    nbLogMsgI(0,'E',"chget: Invalid record encountered. Expecting %d more bytes. Received %d.  errno=%d",expect,i,errno);
    return(-3);
    }
  if(channel->deKey.mode){
    for(i=0;i<(unsigned short)(len/4);i++) channel->buffer[i]=ntohl(channel->buffer[i]);
    /* skeCipher(channel->deCipher,1,channel->deCipher,channel->enKey);*/  /* encrypt cipher */
    skeRandCipher(channel->deCipher);
    skeCipher(channel->buffer,len/16,channel->deCipher,&(channel->deKey));
    checksum=0;                                 /* initialize checksum */
    for(i=0;i<(unsigned short)(len/4-1);i++){
      checksum+=channel->buffer[i];                 /* compute checksum */
      channel->buffer[i]=htonl(channel->buffer[i]);
      }
    if(checksum!=channel->buffer[i]){
      nbLogMsgI(0,'E',"chget: Checksum error.");
      return(-4);
      }
    len-=*((unsigned char *)channel->buffer+len-5);
    }
  //2012-01-31 dtl: replaced memcpy, buffer has sizeof NB_BUFSIZE, safe to copy after test
  //if (len>0 && len<NB_BUFSIZE) for(i=0;i<len;i++) *(buffer+i)=*((char*)channel->buffer+i);  //dtl // 2012-10-13 eat memcpy should be good
  if(len>0 && len<NB_BUFSIZE) memcpy(buffer,channel->buffer,len); // 2012-01-31 dtl - length check 
  else{
    nbLogMsgI(0,'E',"Invalid record encountered. Length = %d",len);
    return(-2);
    } 
  buffer[len]=0; /* null terminate */
  return(len);    
  }

/*
*  Close channel
*/  
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chclose(struct CHANNEL *channel){ 
  channel->enKey.mode=0;
  channel->deKey.mode=0;
#if defined(WIN32)
  return(closesocket(channel->socket));
#else    
  return(close(channel->socket));
#endif    
  }

/*
*  Free channel descriptor 
*/  
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chfree(struct CHANNEL *channel){
  nbFree(channel,sizeof(struct CHANNEL));
  return(0);
  }
