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
extern int chget(struct CHANNEL *channel,char *buffer,size_t size);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chclose(struct CHANNEL *channel);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int chfree(struct CHANNEL *channel);

#endif
