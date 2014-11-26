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
## Program:  NodeBrain (nb)
##
## File:     Makefile.am
##
## Title:    NodeBrain Modules automake input file
##
## Function:
##
##   This file is processed by automake to create the Makefile.in file when
##   a NodeBrain developer is creating a NodeBrain distribution.
##
## Description:
##
##   You will only need this file if you are developing NodeBrain enhancements
##   or improving the install for you platform.
##
##   See http://www.gnu.org/software/automake for more information.
##
##=============================================================================
## Change History:
##
## Date       Name/Change
## ---------- -----------------------------------------------------------------
## 2008/10/27 Ed Trettevik (version 0.7.3 - split out from nb package)
## 2010/02/27 eat 0.7.9  Included DESTDIR
## 2013-03-10 eat 0.8.15 Installing modules directly to version subdirectory
## 2014-02-01 eat 0.9.00 Let the configure script decide which SUBDIRS to build
##===================================o==========================================

SUBDIRS = @NB_MODULES@

EXTRA_DIST = \
  README

#	cp model/Makefile.nm $$subdir/Makefile.nm-; \
.PHONY: model
model:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  cp model/Makefile.am $$subdir/Makefile.am; \
	  mkdir -p $$subdir/caboodle/bin; \
	  cp model/caboodle/bin/nbcheck $$subdir/caboodle/bin/nbcheck; \
	  if test -h "$$subdir/caboodle/bin/nb"; then rm $$subdir/caboodle/bin/nb; fi; \
	done

.PHONY: moddiff
moddiff:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  diff model/Makefile.am $$subdir/Makefile.am; \
	  diff model/caboodle/bin/nbcheck $$subdir/caboodle/bin/nbcheck; \
	done

uninstall-local:
	rm -fr $(libdir)/nb-$(VERSION)
