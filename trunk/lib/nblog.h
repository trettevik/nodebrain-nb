/*
* Copyright (C) 1998-2009 The Boeing Company
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
* File:     nblog.h 
*
* Title:    Log Writting Routine Header
*
* Function:
*
*   This header defines NodeBrain logging routines.
*
* See nblog.c for more information.
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/06/28 Ed Trettevik (original prototype version)
* 2003/03/15 eat 0.5.1  Split code out of nbout.c to make a true header file.
* 2004/11/20 eat 0.6.2  Included output handlers
* 2005/03/26 eat 0.6.2  Included nbLogDump()
* 2005/04/08 eat 0.6.2  API function definitions moved to nbapi.h
* 2009-02-22 eat 0.7.5  Renamed from nbout.h to nblog.h
*=============================================================================
*/
#ifndef _NB_OUT_H_
#define _NB_OUT_H_

#include <nbcell.h>

#if defined(NB_INTERNAL)

extern int trace; 

/* Active output handlers are passed all output */
struct NB_OUTPUT_HANDLER{
  struct NB_OUTPUT_HANDLER *next;
  struct NB_CELL  *context;
  void *session;
  void (*handler)(struct NB_CELL *context,void *session,char *buffer);
  };

/* outCheck() operations */ 
#define NB_CHECK_START 0  /* Start of check script */
#define NB_CHECK_LINE  1  /* Line of check script */
#define NB_CHECK_STOP  2  /* End of check script */

void outStd();
int  outInit();
void outFlush();
void outStream();
void outStamp();
void outData();
void outPut(char *format,...);
void outHex();
//#if defined(WIN32)
//_declspec (dllexport)
//#else
//extern 
//#endif
void outMsg(int msgid,char msgclass,char *format,...);
void outMsgHdr(int msgid,char msgclass,char *format,...);
void outBar();
int  outCheck();
char *outDirName(char *);
char *outLogName(char *);
char *outUserDir(char *);

/*
*  Functions we export for code using nbi.h that should be
*  converted to nb.h but are still using some internal functions
*  and don't have a context to passed to the API function
*/
#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbLogMsgI(int msgNumber,char msgType,char *format,...);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbLogPutI(char * format,...);

#endif // NB_INTERNAL

//***************************************
// External API

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbLogBar(nbCELL context);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbLogDump(nbCELL context,void *data,int len);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbLogFlush(nbCELL context);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbLogHandlerAdd(nbCELL context,void *session,void (*handler)(nbCELL context,void *session,char *buffer));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbLogHandlerRemove(nbCELL context,void *session,void (*handler)(nbCELL context,void *session,char *buffer));

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbLogMsg(nbCELL context,int msgNumber,char msgType,char *format,...);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbLogPut(nbCELL context,char *format,...);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbLogStr(nbCELL context,char* string);

#endif
