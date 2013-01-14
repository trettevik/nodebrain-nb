/*
* Copyright (C) 1998-2011 The Boeing Company
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
*============================================================================
*/
#ifndef _NB_H_
#define _NB_H_

#include <nb/nbstd.h>         // define some standard system stuff

// Supporting interface includes

#include <nb/nbspine.h>       // Spinal API
#include <nb/nbmedulla.h>     // Medulla API
#include <nb/nbtree.h>        // binary tree functions 
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
#include <nb/nbnode.h>        // node routines
#include <nb/nbqueue.h>       // queue routines
#include <nb/nbtls.h>         // TLS routines
#include <nb/nbpeer.h>        // peer routines
#include <nb/nbproxy.h>       // proxy routines
#include <nb/nbmsg.h>         // message routines
#include <nb/nbverb.h>        /* verb structure */
#include <nb/nbwebster.h>     // webster routines
#include <nb/nbmail.h>        // mail routines

#endif // _NB_H_
