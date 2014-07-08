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
* File:     nbmail.h
*
* Title:    Mail API Header
*
* Function:
*
*   This header defines routines that implement the NodeBrain Proxy API.
*
* See nbmail.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2011-11-05 Ed Trettevik - based this API on Cliff's mailer
* 2012-05-20 eat 0.8.9  Replaced socket with nbTLS
* 2012-05-20 eat 0.8.9  Replaced hostname and port with relay (uri)
*=============================================================================
*/
#ifndef _NBMAIL_H_
#define _NBMAIL_H_       /* never again */

extern int mailTrace;          // debugging trace flag for mail routines

typedef struct NB_MAIL_CLIENT{
  char    host[256];// hostname
  char   *relay;    // uri list; e.g. tcp://localhost:25
  nbTLS  *tls;      // tls handle for communication with relay
  int     socket;   // temporarily maintain socket here to test use of tls
  nbCELL  from;     // sender
  nbCELL  to;       // recipient
  nbCELL  topic;    // first part of Subject
  nbCELL  tweet;    // second part of Subject
  nbCELL  form;     // form letter
  } nbMailClient;

extern nbMailClient *nbMailClientCreate(nbCELL context);
extern int nbMailSend(nbCELL context,nbMailClient *client,char *from,char *to,char *topic,char *tweet,char *body);
extern int nbMailSendAlarm(nbCELL context,nbMailClient *client);

#endif
