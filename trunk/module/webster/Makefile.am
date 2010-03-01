## Process this file with automake to produce Makefile.in
## =============================================================================
##
## Copyright (C) 1998-2010 The Boeing Company
##                         Ed Trettevik <eat@nodebrain.org>
##
## NodeBrain is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
##=============================================================================
##
## File:     Makefile.am
##
## Title:    NodeBrain Node Module Automake File
##
## Function:
##
##   This file provides rules that are common to all modules included in a
##   module package.  Within a NodeBrain distribution, a single copy is 
##   maintained in the module/model directory and copied to the module 
##   subdirectories.   
##   
##   This file is processed by automake to create the Makefile.in file when
##   a NodeBrain developer is creating a module for distribution.
##
##   You will only need this file if you are developing NodeBrain enhancements
##   or improving the install for you platform.
##
##   See http://www.gnu.org/software/automake for more information.
##
## Description:
##
##   This file includes Makefile.nm which is a unique file for a given module
##   that looks something like the following.
##
##     nblib_LTLIBRARIES = nb_<module>.la
##     nb_<module>_la_SOURCES = nb_<module>.c
##     nb_<module>_la_CFLAGS  = $(NB_INCLUDE)
##     nb_<module>_la_LDFLAGS = -module --version-info=0:1
##
##     EXTRA_DIST = \
##       caboodle/check/<checkfile1> \
##       caboodle/check/<checkfile2> \
##       ...
##
##   The Makefile.nm file is processed by automake at development time, and
##   by "make" at build time.  By keeping the individual module specific
##   make rules in Makefile.nm, we are able to use a common Makefile.am for
##   all modules. 
##
##   This file is used in combination with a configure.ac file that defines a
##   few helpful variables.
##
##     ${NB_INCLUDE}       - CFLAGS for include library where nb.h is found
##     ${NB}               - nb program name to use for checking
##     ${NB_LIBRARY_PATH}  - path to nb library to use when checking
##     ${NB_MODULE_PATH}   - path to module to use when checking
##
##   These variables enable a module package to work independently, with an
##   install NodeBrain distribution or within with a NodeBrain distribution
##   before installation.
##
##   A caboodle/bin/nbcheck script is included to support nodebrain check
##   running in a test caboodle included in this package.
##
##=============================================================================
## Change History:
##
## Date       Name/Change
## ---------- -----------------------------------------------------------------
## 2008/10/18 Ed Trettevik (version 0.7.2)
## 2010/02/27 eat 0.7.9  Included DESTDIR
##=============================================================================

nblibdir = @libdir@/nb
include Makefile.nm

EXTRA_DIST += \
  README \
  Makefile.nm \
  caboodle/bin/nbcheck

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

## Run a set of tests to check out a build
##
##   Under "make distcheck" we are in a _build subdirectory that does
##   not include our EXTRA_DIST files.  The ${srcdir} value is "..",
##   which we use to copy the necessary files down to _build.  This
##   provides a writable "check" directory and ensures everything
##   executes with the same relative paths as in a real distribution
##   directory

.PHONY: check
check: all 
	@LD_LIBRARY_PATH=${NB_LIBRARY_PATH}:${LD_LIBRARY_PATH}; export LD_LIBRARY_PATH; \
	NB_MODULE_PATH=${NB_MOD_PATH}:${NB_MODULE_PATH}; export NB_MODULE_PATH; \
	if test ! -d "caboodle"; then \
	  tar -C $(srcdir) -cho caboodle | tar -xf -; \
	  chmod -R 755 caboodle; \
	  if test ! -h caboodle/bin/nb; then ln -s ${NB} caboodle/bin/nb; fi; \
	  cd caboodle; bin/nbcheck || exit 1; \
	  cd ..; rm -fr caboodle; \
	else \
	  if test ! -h caboodle/bin/nb; then ln -s ${NB} caboodle/bin/nb; fi; \
	  cd caboodle; bin/nbcheck || exit 1; \
	fi 
