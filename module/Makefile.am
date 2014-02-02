## Process this file with automake to produce Makefile.in
## =============================================================================
##
## Copyright (C) 1998-2014 The Boeing Company
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

.PHONY: model
model:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  cp model/Makefile.am $$subdir/Makefile.am; \
	  cp model/Makefile.nm $$subdir/Makefile.nm-; \
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
