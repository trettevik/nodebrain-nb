/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbservice.h 
*
* Title:    Service Interface
*
* Function:
*
*   This header defines functions to run as a Unix daemon or Windows service.
*   
* See nbservice.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-16 eat 0.5.2  Created to help with modular compiling.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_SERVICE_H_
#define _NB_SERVICE_H_

#if defined(NB_INTERNAL)

#if defined(WIN32)
void nbwCommand(struct NB_CELL *context,char *cmdverb,char *cursor);
#endif

void nbwServiceStopped(void);
void daemonize(void);

#endif // NB_INTERNAL

// External API

extern int  agent;            /* running as daemon */

#if defined(WIN32)
_declspec (dllexport)
#else
extern
#endif
int nbService(int (*serviceMain)(int argc,char *argv[]),int argc,char *argv[]);

#endif

