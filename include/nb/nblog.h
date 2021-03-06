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
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2013-01-11 eat 0.8.13 Checker updates
*=============================================================================
*/
#ifndef _NB_OUT_H_
#define _NB_OUT_H_

#include <nb/nbcell.h>

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

void outStd(char *buffer);
int  outInit(void);
void outFlush(void);
void outStream(int stream,void (*handler)(char *buffer));
void outStamp(void);
void outData(char *data,size_t len);
void outPut(const char *format,...);
void outHex(unsigned int l,void *buf);
//#if defined(WIN32)
//_declspec (dllexport)
//#else
//extern 
//#endif
void outMsg(int msgid,char msgclass,char *format,...);
void outMsgHdr(int msgid,char msgclass,char *format,...);
void outBar(void);
int  outCheck(int option,char *cursor);
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
extern void nbExit(char *format,...);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbAbort(const char *format,...);

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
