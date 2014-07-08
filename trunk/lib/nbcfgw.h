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
