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
## Title:    NodeBrain Library automake input file
##
## Function:
##
##   This file is processed by automake to create the Makefile.in file when
##   a NodeBrain developer is creating a NodeBrain Library distribution.
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
## 2008-10-27 Ed Trettevik (version 0.7.3 - split out from nb package)
## 2011-11-05 eat - included nbmail.c and nbtext.c
## 2013-12-30 eat - included nbset.c and nbset.h
##=============================================================================
     
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ACLOCAL_AMFLAGS= -I m4
#AM_FLAGS = -Wall -g -fstack-protector-all -Wstrict-aliasing=2

# library headers

libnb_adir = @includedir@/nb
libnb_a_HEADERS = \
  include/nb/nb.h \
  include/nb/nbassertion.h \
  include/nb/nbcell.h \
  include/nb/nbclock.h \
  include/nb/nbcmd.h \
  include/nb/nbidentity.h \
  include/nb/nbip.h \
  include/nb/nbkit.h \
  include/nb/nblist.h \
  include/nb/nblistener.h \
  include/nb/nblog.h \
  include/nb/nbmail.h \
  include/nb/nbmedulla.h \
  include/nb/nbmodule.h \
  include/nb/nbmsg.h \
  include/nb/nbnode.h \
  include/nb/nbobject.h \
  include/nb/nbparse.h \
  include/nb/nbpeer.h \
  include/nb/nbproxy.h \
  include/nb/nbqueue.h \
  include/nb/nbrule.h \
  include/nb/nbsentence.h \
  include/nb/nbservice.h \
  include/nb/nbset.h \
  include/nb/nbsource.h \
  include/nb/nbspine.h \
  include/nb/nbstd.h \
  include/nb/nbstream.h \
  include/nb/nbstem.h \
  include/nb/nbsynapse.h \
  include/nb/nbterm.h \
  include/nb/nbtext.h \
  include/nb/nbtls.h \
  include/nb/nbtranslator.h \
  include/nb/nbtree.h \
  include/nb/nbtrick.h \
  include/nb/nbverb.h \
  include/nb/nbwebster.h

# library gets installed 

lib_LTLIBRARIES = libnb.la

libnb_la_CFLAGS = -Iinclude -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector-all -fno-strict-aliasing
libnb_la_LDFLAGS = -release ${PACKAGE_VERSION}

libnb_la_SOURCES = \
  nbassertion.c \
  nbbfi.c \
  nbbind.c \
  nbcall.c \
  nbcell.c \
  nbclock.c \
  nbcmd.c \
  nbcondition.c \
  nbglobal.c \
  nbhash.c \
  nbiconv.c \
  nbidentity.c \
  nbip.c \
  nbkit.c \
  nblist.c \
  nblistener.c \
  nblog.c \
  nbmacro.c \
  nbmail.c \
  nbmath.c \
  nbmedulla.c \
  nbmodule.c \
  nbmsg.c \
  nbnode.c \
  nbobject.c \
  nbpeer.c \
  nbparse.c \
  nbplus.c \
  nbproxy.c \
  nbqueue.c \
  nbreal.c \
  nbregex.c \
  nbrule.c \
  nbsched.c \
  nbsentence.c \
  nbservice.c \
  nbset.c \
  nbsource.c \
  nbspawn.c \
  nbspine.c \
  nbstem.c \
  nbstream.c \
  nbstring.c \
  nbsynapse.c \
  nbsym.c \
  nbterm.c \
  nbtext.c \
  nbtime.c \
  nbtls.c \
  nbtranslator.c \
  nbtree.c \
  nbtrick.c \
  nbverb.c \
  nbwebster.c \
  include/nb/nb.h \
  include/nb/nbi.h \
  include/nb/nbassertion.h \
  include/nb/nbbfi.h \
  include/nb/nbcall.h \
  include/nb/nbcell.h \
  include/nb/nbclock.h \
  include/nb/nbcmd.h \
  include/nb/nbcondition.h \
  include/nb/nbglobal.h \
  include/nb/nbhash.h \
  include/nb/nbiconv.h \
  include/nb/nbidentity.h \
  include/nb/nbip.h \
  include/nb/nbkit.h \
  include/nb/nblist.h \
  include/nb/nblistener.h \
  include/nb/nbmacro.h \
  include/nb/nbmath.h \
  include/nb/nbmedulla.h \
  include/nb/nbmodule.h \
  include/nb/nbmsg.h \
  include/nb/nbnode.h \
  include/nb/nbobject.h \
  include/nb/nblog.h \
  include/nb/nbparse.h \
  include/nb/nbpeer.h \
  include/nb/nbproxy.h \
  include/nb/nbqueue.h \
  include/nb/nbreal.h \
  include/nb/nbregex.h \
  include/nb/nbrule.h \
  include/nb/nbsched.h \
  include/nb/nbsentence.h \
  include/nb/nbservice.h \
  include/nb/nbset.h \
  include/nb/nbsource.h \
  include/nb/nbspawn.h \
  include/nb/nbspine.h \
  include/nb/nbstem.h \
  include/nb/nbstd.h \
  include/nb/nbstream.h \
  include/nb/nbstring.h \
  include/nb/nbsynapse.h \
  include/nb/nbterm.h \
  include/nb/nbtext.h \
  include/nb/nbtime.h \
  include/nb/nbtls.h \
  include/nb/nbtranslator.h \
  include/nb/nbtree.h \
  include/nb/nbtrick.h

# pcre.h \
# readline/chardefs.h \
# readline/history.h \
# readline/keymaps.h \
# readline/readline.h \
# readline/rlconf.h \
# readline/rlstdc.h \
# readline/rltypedefs.h \
# readline/tilde.h

noinst_PROGRAMS = nb

#bin_PROGRAMS = nb

nb_CFLAGS = -Wall -Iinclude
nb_SOURCES = nb.c

#nb_LDADD = .libs/libnb.so
nb_LDADD = .libs/libnb.la
#nb_DEPENDENCIES = .libs/libnb-${PACKAGE_VERSION}.so
nb_DEPENDENCIES = libnb.la

EXTRA_DIST = nbcfgw.h

EXTRA_DIST+= \
  caboodle/.nb/caboodle.nb \
  caboodle/bin/nbcheck \
  caboodle/check/cellStaticBoolFalse.nb~ \
  caboodle/check/cellStaticBoolTrue.nb~ \
  caboodle/check/cellStaticBoolUnknown.nb~ \
  caboodle/check/cellStaticOpPrec.nb~ \
  caboodle/check/cellStaticRelFalse.nb~ \
  caboodle/check/cellStaticRelTrue.nb~ \
  caboodle/check/cellStaticRelUnknown.nb~ \
  caboodle/check/modules.nb \
  caboodle/check/ruleFireBoolRelEq.nb~ \
  caboodle/check/ruleFireBoolSimple.nb~ \
  caboodle/check/build.nb \
  caboodle/check/README

## Run a set of tests to check out a build

.PHONY: check
check: all
	if test ! -h caboodle/bin/nb; then ln -s ../../nb caboodle/bin/nb; fi
	cd caboodle; bin/nb +bU check/build.nb
	cd caboodle; bin/nbcheck
