/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
