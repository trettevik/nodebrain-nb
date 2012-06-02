/*
* Copyright (C) 2009-2012 The Boeing Company
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
* File:     nbtls.h
*
* Title:    NodeBrain Transport Layer Security Header
*
* Purpose:
*
*   This file provides function headers for Transport Layer Security
*   using the OpenSSL library.
*
* See nbtls.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2009-12-12 eat 0.7.7  (original prototype)
*=============================================================================
*/
#ifndef _NB_TLS_H_
#define _NB_TLS_H_

#include <nb-tls.h>
#include <nb.h>

nbTLSX *nbTlsLoadContext(nbCELL context,nbCELL tlsContext,void *handle,int client);
extern nbTLS *nbTlsLoadListener(nbCELL context,nbCELL tlsContext,char *defaultUri,void *handle);
extern int nbTlsConnectNonBlockingAndSchedule(nbCELL context,nbTLS *tls,void *handle,void (*handler)(nbCELL context,int sd,void *handle));

#endif
