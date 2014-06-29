/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     configw.h
*
* Title:    NodeBrain Windows Configuration Header
*
* Function:
*  
*   This header defines configuration variables on Windows that are defined
*   in config.h on other platforms.  The config.h file is generated on other
*   platforms by the configure script on platforms where we use autoconf and
*   automake.  On Windows we are using a Microsoft Visual Studio workspace,
*   so this file has been created manually, setting only those variables we
*   actually use in our source code.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- ---------------------------------------------------------------
* 2005-04-25 Ed Trettevik (inserted these comments and removed unused definitions)
* 2005-04-25 eat 0.6.2  Included NB_MOD_PATH and NB_MOD_PATH_SEPARATOR
* 2009-02-21 eat 0.7.5  new version
*============================================================================
*/

/* Define compile platform */
#define NB_COMPILE_PLATFORM "x86-windows-xp"

/* Module path */         
#define NB_MODULE_PATH "/Program Files/NodeBrain"

/* Module path */         
#define NB_MODULE_PATH_SEPARATOR ';'

/* Module suffix */
#define NB_MODULE_SUFFIX ".dll"
#define LT_MODULE_EXT ".dll"

/* Define release date */
#define NB_RELEASE_DATE "2009/02/20"

/* Name of package */
#define PACKAGE "nb"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bugs@nodebrain.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "nb"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "nb 0.7.5"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "nb"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.7.5"

// Define the API version
#define NB_API_VERSION "0"
