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
* File:     nbregex.h 
*
* Title:    Regular Expression Funtion Header
*
* Function:
*
*   This header defines routines that manage nodebrain regular expression
*   objects.  We use standard regular expression as supported by regex.h, but
*   package them here to fit into the nodebrain object scheme.
*  
* See nbregex.c for more information.
*===============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ------------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Split out *.h and *.c files for new make file
* 2003-03-17 eat 0.5.2  Condition out Windows here instead of at include
* 2004-10-02 eat 0.6.1  Preparing to support on Windows - not really yet  
* 2008-10-06 eat 0.7.2  Included flags to support different compile options
* 2008-11-06 eat 0.7.3  Converting to PCRE's native API
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2014-01-12 eat 0.9.00 Dropped hash pointer - access via type
*===============================================================================
*/
#ifndef _NB_REGEX_H_
#define _NB_REGEX_H_

#include <pcre.h>           // Perl Compatible Regular Expression (PCRE) Library

extern struct TYPE *regexpType;   /* regular expression object type */
extern struct REGEXP *freeRegexp; /* free structure pool */

struct REGEXP{              // Regular expression
  struct NB_OBJECT object;  // object header
  struct STRING *string;    // character string representation
  int flags;                // flags passed to regcomp()
  pcre *re;                 // compiled regular expression
  pcre_extra *pe;           // extra data returned by pcre_study()
  int nsub;                 // number of sub expressions
  };
  
struct REGEXP *newRegexp(char *expression,int flags);
void printRegexp(struct REGEXP *regexp);
void destroyRegexp(struct REGEXP *regexp);
void initRegexp(NB_Stem *stem);

#endif
