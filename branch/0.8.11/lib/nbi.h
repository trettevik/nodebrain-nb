/*
* Copyright (C) 1998-2012 The Boeing Company
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

#include <nbstd.h>         // define some standard system stuff
#include <nbhash.h>        /* hash table of objects */
//#include <nbverb.h>        /* verb structure */
#include <nbstring.h>      /* string object */
#include <nbtext.h>        /* text object */

#include <nbglobal.h>      /* global variables */

#include <nbspawn.h>       /* routines that spawn child processes */
#include <nbiconv.h>       /* EBCDIC/ASCII conversion */

#include <nbreal.h>        /* real object */
#include <nbregex.h>      /* regular expression object */
#include <nbcondition.h>   /* condition object */
#include <nbclock.h>
#include <nbbfi.h>         /* binary function of integer */
#include <nbtime.h>        /* time conditions */
#include <nbsched.h>       /* schedule and old time conditions */
#include <nbcall.h>        /* built-in function calls */
#include <nbmath.h>        /* math functions */

#include <nbparse.h>       /* parsing functions */

#include <nbmodule.h>      /* module object */
#include <nbmacro.h>       /* string macro object */

#include <nb.h>

#endif
