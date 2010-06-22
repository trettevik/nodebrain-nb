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
*============================================================================
*/
#ifndef _NB_H_
#define _NB_H_

#include <nbstd.h>         // define some standard system stuff

// Supporting interface includes

#include <nbspine.h>       // Spinal API
#include <nbmedulla.h>     // Medulla API
#include <nbtree.h>        // binary tree functions 
#include <nbip.h>          // ip related function

// Interpreter related includes

#include <nbstem.h>        /* stem cell */
#include <nblog.h>         /* log writing functions */
#include <nbobject.h>      /* object prefix and object types */
#include <nblist.h>        /* list function */
#include <nbcell.h>        /* cell object */
#include <nbterm.h>        /* name, definition, value binding object */
#include <nbsynapse.h>     /* synapse object */
#include <nbnode.h>        /* node object */
#include <nbidentity.h>    /* identity object */
#include <nbservice.h>     /* service routines */
#include <nbstream.h>      /* data stream */
#include <nbassertion.h>   /* assertion objects */
#include <nbrule.h>        /* rules */
#include <nbtranslator.h>  /* text translator */
#include <nblistener.h>    /* listener object */
#include <nbcmd.h>         /* command processor */
// 2009-02-14 eat 0.7.5 - exposing external API
#include <nbmodule.h>      // module routines
#include <nbparse.h>       // parsing routines
#include <nbclock.h>       // clock routines
#include <nbrule.h>        // rule routines
#include <nbnode.h>        // node routines
#include <nbqueue.h>       // queue routines
#include <nbtls.h>         // TLS routines
#include <nbpeer.h>        // peer routines
#include <nbmsg.h>         // message routines
#include <nbverb.h>        /* verb structure */

#endif // _NB_H_
