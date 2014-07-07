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
* 2008-10-23 Ed Trettevik (version 0.7.3 - extracted from old nb.h)
*            nbi.h replaces old nb.h and new nb.h replaces old nbapi.h
* 2014-01-12 eat 0.9.00 Included nbsentence and nbaxon headers
*============================================================================
*/
#ifndef _NB_I_H_
#define _NB_I_H_

#define NB_INTERNAL

#define _USE_32BIT_TIME_T

#if defined(WIN32)
#include <nbcfgw.h>       /* tweeked config file */
#else
#include "../config.h"        /* Definitions provided by configure script */
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
#include <nb/nbregex.h>       /* regular expression object */
#include <nb/nbcondition.h>   /* condition object */
#include <nb/nbconditional.h> /* conditional object */
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
#include <nb/nbaxon.h>

#include <nb/nb.h>

#endif
