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
* File:     nbstd.h
*
* Title:    NodeBrain Standard Headers
*
* Function:
*
*   This header includes standard headers required by multiple nb*.c files. It
*   provide a single file in which to worry about platform variations.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2003/03/15 eat 0.5.1  nbstd.h extracted from nb.h
* 2003/10/07 eat 0.5.5  removed definition of nbin - see nbGets().
* 2005/05/01 eat 0.6.2  fiddled with sys/time.h and time.h conditional a bit
*                SF Debian 2.2 system needed sys/time.h for timeval
* 2005/06/10 eat 0.6.3  included sys/types.h
* 2006/01/16 eat 0.6.4  juggled around a bit
* 2008/11/06 eat 0.7.3  Converfted to PCRE's native API
* 2008/11/11 eat 0.7.3  Added NB_EXITCODE_* definitions
* 2012-12-25 eat 0.8.13 Included langinfo.h and locale.h
* 2014-02-16 eat 0.9.01 Included sys/un.h  (also in 0.8.16)
*============================================================================
*/
#ifndef _NB_STD_H_
#define _NB_STD_H_

#define NB_MSGSIZE 1024          // MIN msg buffer size 
#define NB_BUFSIZE 16*1024       // Use 16KB buffers
#define NB_EXITCODE_BAIL    254  // User request exit on error message
#define NB_EXITCODE_FAIL    255  // Unrequested failure

// Common for Windows and Unix/Linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>   /* version 0.4.1 */
#include <ctype.h>  // 2013-04-06 eat - character type functions

#if defined(WIN32)

//#define _WIN32_WINNT 0x0400
#define _WIN32_WINNT 0x0502
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <winsvc.h>
#include <shlobj.h>
#include <io.h>
#include <process.h>
//#include <time.h>
//#include <regexw.h>  // version 0.6.2 - rxspencer.dll from GnuWin32 project

#else  // not windows

#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>    // 0.6.4
#include <unistd.h>      // 0.6.4
#include <sys/un.h>      // 2014-02-16 eat
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif
#if defined(HAVE_SYS_LIMITS_H)
#include <sys/limits.h>
#endif
#if defined(HAVE_MACHINE_LIMITS_H)
#include <machine/limits.h>
#endif
#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>  // 0.6.4
#endif
/* 2004/10/06 Order of in.h and inet.h changed to compile on OpenBSD */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#if defined(TIME_WITH_SYS_TIME) || !defined(HAVE_SYS_TIME_H)
#include <time.h>
#endif

#include <locale.h>       // 2012-12-25 eat - 0.8.13
#include <langinfo.h>     // 2012-12-25 eat - 0.8.13 

//#include <regex.h>      /* version 0.2.8 */
//#include <pcreposix.h>  /* version 0.6.7 */

//#include <sys/dir.h>  // in /usr/ucbinclude/sys/dir.h on our solaris machine
#include <dirent.h>
//#include <sys/dirent.h>  // experiment on solaris

#endif

//#include <pcre.h>         /* version 0.7.3 */

#endif
