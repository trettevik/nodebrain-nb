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
* File:     nbparse.h 
*
* Title:    Header for Parsing Functions 
*
* See nbparse.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-08-25 Ed Trettevik (original version split out of nblogic.c)
* 2005-04-08 eat 0.6.2  API function definitions moved to nbapi.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_PARSE_H_
#define _NB_PARSE_H_

#if defined(NB_INTERNAL)

extern int parseTrace;        /* debugging trace flag */

extern unsigned char nb_CharClass[256];

#define NB_CHAR_NUMBER    0  // numeric
#define NB_CHAR_ALPHA     1  // alpha
                             // alphanumberic <=1
#define NB_CHAR_TERMQUOTE 2  // single quote - used for quoted terms
#define NB_CHAR_QUOTE     3  // quote - used for quoted strings
#define NB_CHAR_LEADING   4  // leading symbol for terms
#define NB_CHAR_RELATION  5  // relational operator
#define NB_CHAR_TILDE     6  // ~ - operators starting with tilde
#define NB_CHAR_NOT       7  // ! - operators starting with not
#define NB_CHAR_COMBO     8  // characters starting other operator symbols
#define NB_CHAR_SOLO      9  // characters not combined with others
#define NB_CHAR_DELIM    11  // cell expression delimiter
#define NB_CHAR_END      12  // end of text

#define NB_ISNUMERIC(CHAR) (nb_CharClass[CHAR]==NB_CHAR_NUMBER)
#define NB_ISALPHA(CHAR) (nb_CharClass[CHAR]==NB_CHAR_ALPHA)
#define NB_ISALPHANUMERIC(CHAR) (nb_CharClass[CHAR]<=NB_CHAR_ALPHA)
#define NB_ISCELLDELIM(CHAR) (nb_CharClass[CHAR]==NB_CHAR_DELIM || nb_CharClass[CHAR]==NB_CHAR_END)
#define NB_ISSYMDELIM(CHAR) (CHAR==' ' || nb_CharClass[CHAR]==NB_CHAR_DELIM || nb_CharClass[CHAR]==NB_CHAR_END)

void nbParseInit(void);
NB_Object *nbParseObject(NB_Term *context,char **cursor);
NB_Object *nbParseRel(NB_Term *context,char **cursor);
NB_Object *nbParseCell(NB_Term *context,char **cursor,int level);
struct NB_LINK *nbParseAssertion(NB_Term *leftContext,NB_Term *rightContext,char **curP);


char *my_strtok_r(char *cursor,char *delim,char **newcur);
int isAlpha(char c);
int isNumeric(char c);
int isAlphaNumeric(char c);
char *getQualifier(char *qCursor,char *sCursor);

#endif // NB_INTERNAL

//**********************************
// External API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char nbParseSymbol(char *ident,char **cursor);

#endif
