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
* File:     nb.h
*
* Title:    NodeBrain Header
*
* Function:
*
*   This header includes other headers to enable a single include for
*   access to NodeBrain's C API provided by the NodeBrain Library.  This
*   header should be included by NodeBrain node modules and other programs
*   like nb that use the NodeBrain Library.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2003/02/13 Ed Trettevik (version 0.5.0  Split out from nb.c)
*
* 2003/03/08 eat 0.5.1  Included nblog.h and nblog.c
*                       These files support the LOG listener.
* 2003/03/16 eat 0.5.2  Cleaned up by creating nbstd.h and nbglobal.h
* 2004/03/25 eat 0.6.0  Included config.h
* 2005/03/26 eat 0.6.2  Included nbip.h
* 2005/04/10 eat 0.6.2  Revised for enhanced API
* 2005/05/06 eat 0.6.2  Changed config.h to nbcfg.h
* 2008/03/08 eat 0.7.0  Removed nblog.h, nbpipe.h
* 2008/03/08 eat 0.7.0  Removed NBP related headers
* 2008/10/23 eat 0.7.3  This nb.h replaces the old nbapi.h
* 2009-02-22 eat 0.7.5  Renamed nbout.h to nblog.h
* 2009-12-12 eat 0.7.7  Included nbmsg.h
* 2011-11-05 eat 0.8.6  Included nbmail.h
* 2014-02-16 eat 0.9.01 Conditional OpenSSL headers (also in 0.8.16)
*============================================================================
*/
#ifndef _NB_H_
#define _NB_H_

#include <nb/nbstd.h>         // define some standard system stuff

#include <nb/nbkit.h>         // nbkit command support

// Supporting interface includes

#include <nb/nbspine.h>       // Spinal API
#include <nb/nbmedulla.h>     // Medulla API
#include <nb/nbtree.h>        // binary tree functions 
#include <nb/nbset.h>         // set binary tree functions 
#include <nb/nbip.h>          // ip related function

// Interpreter related includes

#include <nb/nbstem.h>        /* stem cell */
#include <nb/nblog.h>         /* log writing functions */
#include <nb/nbobject.h>      /* object prefix and object types */
#include <nb/nblist.h>        /* list function */
#include <nb/nbcell.h>        /* cell object */
#include <nb/nbterm.h>        /* name, definition, value binding object */
#include <nb/nbsynapse.h>     /* synapse object */
#include <nb/nbnode.h>        /* node object */
#include <nb/nbidentity.h>    /* identity object */
#include <nb/nbservice.h>     /* service routines */
#include <nb/nbstream.h>      /* data stream */
#include <nb/nbassertion.h>   /* assertion objects */
#include <nb/nbrule.h>        /* rules */
#include <nb/nbtranslator.h>  /* text translator */
#include <nb/nblistener.h>    /* listener object */
#include <nb/nbsource.h>      /* rule file sourcing */
#include <nb/nbcmd.h>         /* command processor */
// 2009-02-14 eat 0.7.5 - exposing external API
#include <nb/nbmodule.h>      // module routines
#include <nb/nbparse.h>       // parsing routines
#include <nb/nbclock.h>       // clock routines
#include <nb/nbrule.h>        // rule routines
//#include <nb/nbnode.h>        // node routines
#include <nb/nbqueue.h>       // queue routines
#include <nb/nbverb.h>        /* verb structure */

#ifdef HAVE_OPENSSL
#include <nb/nbtls.h>         // TLS routines
#include <nb/nbpeer.h>        // peer routines
#include <nb/nbproxy.h>       // proxy routines
#include <nb/nbmsg.h>         // message routines
#include <nb/nbwebster.h>     // webster routines
#include <nb/nbmail.h>        // mail routines
#endif

#endif // _NB_H_
