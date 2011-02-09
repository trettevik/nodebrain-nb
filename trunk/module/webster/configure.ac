# Process this file with autoconf to produce a configure script.
#=============================================================================
#
# Copyright (C) 1998-2009 The Boeing Company
#                         Ed Trettevik <eat@nodebrain.org>
#
# NodeBrain is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#=============================================================================
# Program:  NodeBrain
#
# File:     configure.ac
#
# Title:    NodeBrain Node Module Autoconf Input File
#
# Function:
#
#   This file is processed by autoconf to create the configure script when
#   a NodeBrain developer is creating a node module distribution.
#
# Description:
#
#   You will not need this file unless you are attempting to use autoconf
#   to build a new configure script to resolve problems on your platform.
#   If so, you would do something like the following.
#
#     o  Modify this file (configure.ac) as necessary.
#     o  Execute "make distclean" if you have already run "make".  This
#        will clean up files created previously by "./configure" and "make"
#        that are not part of the NodeBrain distribution.
#     o  Execute autoconf in this directory to create a new configure script.
#     o  Execute ./configure to create a new Makefile and config.h file.
#
#   See http://www.gnu.org/software/autoconf for more information.
#
#=============================================================================
# Change History:
#
# Date       Name/Change
# ---------- -----------------------------------------------------------------
# 2008-11-02 Ed Trettevik (version 0.7.3)
# 2009-02-20 eat 0.7.5 port to other platforms (windows plus others)
# 2010-02-26 John Ellson - fix libtoolize warning
#=============================================================================

AC_PREREQ(2.62)
AC_INIT([nb_webster],[0.0.2],[bugs@nodebrain.org])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE

#=====================================
# LIBTOOL stuff

LT_PREREQ([2.2.4])
LT_INIT([dlopen])
LT_SYS_MODULE_EXT
AC_SUBST([LIBTOOL_DEPS])

#=====================================
# Checks for programs.

AC_PROG_CC
AC_PROG_CXX

# Figure out which nb program to use with "make check".
#   If we are in a Nodebrain distribution, we'll use
#   the one we created in the top directory.  Otherwise,
#   we look for a compatible installed program.

AC_SUBST([NB],[../../../../lib/.libs/lt-nb])
#AC_CHECK_PROG([NB],[nb.h],[../../../../lib/.libs/lt-nb],,[../../lib])
#if test "x${NB}" = "x"; then
#  AC_PATH_PROGS([NB],[nb-0.7.5 nb-0.7.4 nb-0.7.3 nb-0.7.2 nb-0.7.1 nb])
#fi

CFLAGS="-Wall $CFLAGS"

#=====================================
# Checks for libraries.

# Find -lssl library

AC_SEARCH_LIBS([SSL_CTX_new],[ssl],,[AC_MSG_ERROR(Required library -lssl not found. You may want to download it from http://www.openssl.org or locate it and include the directory in LD_LIBRARY_PATH for this build.)],[-lcrypto])
#AC_SEARCH_LIBS([SSL_CTX_new],[ssl],,[AC_MSG_ERROR(Required library -lssl not found. You may want to download it from http://www.openssl.org or locate it and include the directory in LD_LIBRARY_PATH for this build.)])

# Figure out which NB_LIBRARY_PATH to use with "make check".
#   If we are in a NodeBrain distribution, we'll use
#   the library we created in the lib directory.  Otherwise
#   we'll the installed one, which may be installed somewhere
#   else if already in the search path.

AC_CHECK_FILE([../../lib/libnb.la],[AC_SUBST([NB_LIBRARY_PATH],[../../../lib/.libs])],[AC_SUBST([NB_LIBRARY_PATH],[/usr/local/lib/nb])])

# Always provide a path to the module we created in this package

AC_SUBST([NB_MODULE_PATH],[../.libs])

#=====================================
# Checks for header files.

# Figure out which nb.h to use
#   If we are in a NodeBrain distribution, we'll use
#   the one we created in the lib directory.  Otherwise
#   we'll use the installed one.

AC_CHECK_FILE([../../lib/nb.h],[AC_SUBST([NB_INCLUDE],[-I../../lib])],[AC_SUBST([NB_INCLUDE],[-I/usr/local/include/nb])])

#=====================================
# Checks for typedefs, structures, and compiler characteristics.

#=====================================
# Checks for library functions.

AC_FUNC_MALLOC
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([setenv])

#=====================================
# Create the Makefile

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
