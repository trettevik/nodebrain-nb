## Process this file with automake to produce Makefile.in
## =============================================================================
##
## Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
##
## NodeBrain is free software; you can modify and/or redistribute it under the
## terms of either the MIT License (Expat) or the following NodeBrain License.
##
## Permission to use and redistribute with or without fee, in source and binary
## forms, with or without modification, is granted free of charge to any person
## obtaining a copy of this software and included documentation, provided that
## the above copyright notice, this permission notice, and the following
## disclaimer are retained with source files and reproduced in documention
## included with source and binary distributions. 
##
## Unless required by applicable law or agreed to in writing, this software is
## distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
## KIND, either express or implied.
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
##     nb_<module>_la_CFLAGS  = $(NB_MOD_CFLAGS)
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
##     ${NB_MOD_CFLAGS}    - CFLAGS common to all modules
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
## 2008-10-18 Ed Trettevik (version 0.7.2)
## 2013-03-10 eat 0.8.15 Modified nblibdir to place module in minor version directory
##=============================================================================

nblibdir = ${libdir}/nb-${NB_MOD_DIR}
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
