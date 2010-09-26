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
##=============================================================================
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = audit baseline cache console mail message netflow peer pipe servant snmptrap string syslog toy translator tree udp webster

EXTRA_DIST = \
  README

.PHONY: model
model:
	list='$(SUBDIRS) duplicator junction'; for subdir in $$list; do \
	  cp model/Makefile.am $$subdir/Makefile.am; \
	  cp model/Makefile.nm $$subdir/Makefile.nm-; \
	  mkdir -p $$subdir/caboodle/bin; \
	  cp model/caboodle/bin/nbcheck $$subdir/caboodle/bin/nbcheck; \
	  if test -h "$$subdir/caboodle/bin/nb"; then rm $$subdir/caboodle/bin/nb; fi; \
	done

.PHONY: moddiff
moddiff:
	list='$(SUBDIRS) duplicator junction'; for subdir in $$list; do \
	  diff model/configure.ac $$subdir/configure.ac; \
	done

# Construct a minor version module directory
#    /usr/local/lib/nb/x.y/nb_<module>.so.x.y.z   where x=major y=minor and z=patch
#
install-exec-hook:
	minor=`echo $(VERSION) | sed 's/\([0-9]*.[0-9]*\).[^ ]*/\1/'`; \
	if test ! -d "${DESTDIR}@libdir@/nb/$$minor"; then mkdir -p ${DESTDIR}@libdir@/nb/$$minor; fi; \
	ext=`grep LT_MODULE_EXT config.h | sed 's/[^"]*"\([^"]*\)"/\1/'`; \
	major=`echo $$minor | sed 's/\([0-9]*\).[^ ]*/\1/'`; \
	list="$(SUBDIRS)"; \
	for subdir in $$list; do \
	  module=`ls -al $$subdir/.libs/nb_$$subdir$$ext | sed 's/.*nb_[^ ]*.*\(nb\_.*\)/\1/'`; \
	  test -h "${DESTDIR}@libdir@/nb/$$minor/nb_$$subdir$$ext.$$major" && rm ${DESTDIR}@libdir@/nb/$$minor/nb_$$subdir$$ext.$$major; \
	  ln -s ../$$module ${DESTDIR}@libdir@/nb/$$minor/nb_$$subdir$$ext.$$major; \
	  done

uninstall-local:
	rm -fr $(libdir)/nb/$(VERSION)
