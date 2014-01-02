/*
* Copyright (C) 1998-2013 The Boeing Company
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbi.h
*
* Title:    NodeBrain Header
*
* Function:
*
*   Single header for use within the NodeBrain Library.  This header is not
*   installed because it exposes internals that are not part of the external
*   API and may change at any time.
*   
* Description:
*
*   Headers that do not support the external API are included first.  Then
*   we include nb.h after defining NB_INTERNAL.  This causes headers included
*   by nb.h to expose the internal API.  Modules and other users of the
*   NodeBrain Library include nb.h without NB_INTERNAL so only the external
*   API is exposed.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2008/10/23 Ed Trettevik (version 0.7.3 - extracted from old nb.h)
*            nbi.h replaces old nb.h and new nb.h replaces old nbapi.h
*============================================================================
*/
#ifndef _NB_I_H_
#define _NB_I_H_

#define NB_INTERNAL

#define _USE_32BIT_TIME_T

#if defined(WIN32)
#include <nbcfgw.h>       /* tweeked config file */
#else
#include <config.h>        /* Definitions provided by configure script */
#endif

#include <nb/nbstd.h>      // define some standard system stuff
#include <nb/nbtree.h>        /* tree of objects */
#include <nb/nbset.h>         /* set of objects */
#include <nb/nbhash.h>        /* hash table of objects */
//#include <nb/nbverb.h>        /* verb structure */
#include <nb/nbstring.h>      /* string object */
#include <nb/nbtext.h>        /* text object */

#include <nb/nbglobal.h>      /* global variables */

#include <nb/nbspawn.h>       /* routines that spawn child processes */
#include <nb/nbiconv.h>       /* EBCDIC/ASCII conversion */

#include <nb/nbreal.h>        /* real object */
#include <nb/nbregex.h>      /* regular expression object */
#include <nb/nbcondition.h>   /* condition object */
#include <nb/nbclock.h>
#include <nb/nbbfi.h>         /* binary function of integer */
#include <nb/nbtime.h>        /* time conditions */
#include <nb/nbsched.h>       /* schedule and old time conditions */
#include <nb/nbcall.h>        /* built-in function calls */
#include <nb/nbmath.h>        /* math functions */

#include <nb/nbparse.h>       /* parsing functions */

#include <nb/nbmodule.h>      /* module object */
#include <nb/nbmacro.h>       /* string macro object */

#include <nb/nbsentence.h>
#include <nb/nbtrick.h>

#include <nb/nb.h>

#endif
