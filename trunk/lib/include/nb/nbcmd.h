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
* File:     nbcmd.h
*
* Title:    Command Processor Header (prototype)
*
* Function:
*
*   This header defines routines that parse and execute NodeBrain commands.
*
* See nbcmd.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-16 Ed Trettevik - (split out from nb.c in 0.5.2)
* 2004-08-29 eat 0.6.1  Included command option flags
* 2005-04-09 eat 0.6.2  Move API function nbcmd() definition to nbapi.h
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NB_CMD_H_
#define _NB_CMD_H_

#include <nb/nbidentity.h>

#if defined(NB_INTERNAL) 

void printVersion(void);
void printHelp(void);

void showVersion(void);
void showCopyright(void);
void showHeading(void);
int nbCmdUse(struct NB_CELL *context,void *handle,char *verb,char *cursor);
int nbCmdSet(struct NB_CELL *context,void *handle,char *verb,char *cursor);
int nbCmdQuery(struct NB_CELL *context,void *handle,char *verb,char *cursor);
void nbParseArgAssertion(char *cursor);

void nbParseStdin(int prompt);

int nbParseSourceTil(struct NB_CELL *context,FILE *file);
int nbParseSourceIgnoreTil(struct NB_CELL *context,FILE *file,char *buf,int til);
 
/* symbolic substitution routines */
extern char *nb_symBuf1;
extern char *nb_symBuf2;

char *nbSymCell(NB_Cell *context,char **target,char *targetend,char *source,char stop);
//void nbSymReduce(char *source,char *target,char sym,char open);
char *nbSymCmd(NB_Cell *context,char *source,char *style);

char *nbSymSource(NB_Cell *context,char *source);
void nbCmdInit(struct NB_STEM *stem);
void printAbout(void);
int nbLet(char *cursor,NB_Term *context,int mode);

#endif // !NB_INTERNAL (external API)

/* Command Control Options
*
*    ECHO  - use if you want the command to be displayed to the log
*    HUSH  - use if command was displayed before calling nbCmd()
*            HUSH trumps ECHO
*    TRACE - use if you want debugging info displayed
*            TRACE trumps HUSH
*/
#define NB_CMDOPT_ECHO      1 // Request echo
#define NB_CMDOPT_HUSH      2 // Suppress echo - already echoed
#define NB_CMDOPT_TRACE     4 // Display debugging info
#define NB_CMDOPT_RULE      8 // Display rule action with ":" in column one
#define NB_CMDOPT_ALERT   128 // Indicates an action assertion is an alert

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbParseSource(nbCELL context,char *cursor);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbCmd(nbCELL context,char *cursor,unsigned char option);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbCmdSid(nbCELL context,char *cursor,char cmdopt,nbIDENTITY);


#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbGets(int file,char *strbuf,size_t strlen);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern char *nbGetReply(char *prompt,char *buffer,size_t len);

#endif 
