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
* Program:  NodeBrain
*
* File:     nbchannel.h 
* 
* Title:    Channel Layer Protocol Header
*
* Function:
*
*   This header defines routines used for TCP/IP socket communication
*   with secret key encryption.
*
* See nbchannel.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Created to conform to new makefile
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_CHANNEL_H_
#define _NB_CHANNEL_H_

#include "nbske.h"

struct CHANNEL{
  skeKEY   enKey;            /* Encryption key */
  skeKEY   deKey;            /* Decryption key */
  unsigned int enCipher[4];  /* Cipher Block Chaining (CBC) encipher key */
  unsigned int deCipher[4];  /* Cipher Block Chaining (CBC) decipher key */
  int socket;                /* socket number or handle */
  char ipaddr[16];           /* ip address of peer */
  char unaddr[256];          // local (unix) domain socket path
  unsigned short port;       /* port to communicate with */
  unsigned short len;        /* Buffer length in bytes */
  unsigned int buffer[NB_BUFSIZE]; /* buffer - must follow len */
  /*  char[*] text         - plaintext or ciphertext */
  /*  char[5-16] trailer   - fill last 16 byte (128 bit) encryption block */
  /*    char[0-11]            - padding filled with random characters */
  /*    char[1]               - trailer length */
  /*    unsigned int          - checksum */
  /*  Note: The trailer is only used with encryption */
  };

void chkey(struct CHANNEL *channel,skeKEY *enKey,skeKEY *deKey,unsigned int enCipher[4],unsigned int deCipher[4]);

char *chgetaddr(char *hostname);
char *chgetname(char *ipaddr);

/* 
*  API Functions
*
*  We should rename the routines nbChannelXxxx() or nbComXxxx() before 
*  documenting the API
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chopen(struct CHANNEL *channel,char *ipaddr,unsigned short port);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chput(struct CHANNEL *channel,char *buffer,size_t len);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chputmsg(struct CHANNEL *channel,char *buffer,size_t len);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chstop(struct CHANNEL *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chlisten(char *ipaddr,unsigned short port);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void chclosesocket(int serverSocket);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern struct CHANNEL *challoc(void);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chaccept(struct CHANNEL *channel,int serverSocket);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chget(struct CHANNEL *channel,char *buffer);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chclose(struct CHANNEL *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chfree(struct CHANNEL *channel);

#endif
