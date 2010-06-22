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
## 2008/10/27 Ed Trettevik (version 0.7.3 - split out from nb package)
##=============================================================================
     
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ACLOCAL_AMFLAGS= -I m4

# library headers

libnb_adir = @includedir@/nb
libnb_a_HEADERS = \
  nb.h \
  nbassertion.h \
  nbcell.h \
  nbclock.h \
  nbcmd.h \
  nbidentity.h \
  nbip.h \
  nblist.h \
  nblistener.h \
  nblog.h \
  nbmedulla.h \
  nbmodule.h \
  nbmsg.h \
  nbnode.h \
  nbobject.h \
  nbparse.h \
  nbpeer.h \
  nbqueue.h \
  nbrule.h \
  nbservice.h \
  nbspine.h \
  nbstd.h \
  nbstream.h \
  nbstem.h \
  nbsynapse.h \
  nbterm.h \
  nb-tls.h \
  nbtls.h \
  nbtranslator.h \
  nbtree.h \
  nbverb.h

# library gets installed 

#lib_LTLIBRARIES = libnb.la libnbp.la
lib_LTLIBRARIES = libnb.la

libnb_la_LDFLAGS= -release ${PACKAGE_VERSION}

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
  nblist.c \
  nblistener.c \
  nblog.c \
  nbmacro.c \
  nbmath.c \
  nbmedulla.c \
  nbmodule.c \
  nbmsg.c \
  nbnode.c \
  nbobject.c \
  nbpeer.c \
  nbparse.c \
  nbplus.c \
  nbqueue.c \
  nbreal.c \
  nbregex.c \
  nbrule.c \
  nbsched.c \
  nbservice.c \
  nbspawn.c \
  nbspine.c \
  nbstem.c \
  nbstream.c \
  nbstring.c \
  nbsynapse.c \
  nbsym.c \
  nbterm.c \
  nbtime.c \
  nb-tls.c \
  nbtls.c \
  nbtree.c \
  nbtranslator.c \
  nbverb.c \
  nb.h \
  nbi.h \
  nbassertion.h \
  nbbfi.h \
  nbcall.h \
  nbcell.h \
  nbclock.h \
  nbcmd.h \
  nbcondition.h \
  nbglobal.h \
  nbhash.h \
  nbiconv.h \
  nbidentity.h \
  nbip.h \
  nblist.h \
  nblistener.h \
  nbmacro.h \
  nbmath.h \
  nbmedulla.h \
  nbmodule.h \
  nbmsg.h \
  nbnode.h \
  nbobject.h \
  nblog.h \
  nbparse.h \
  nbpeer.h \
  nbqueue.h \
  nbreal.h \
  nbregex.h \
  nbrule.h \
  nbsched.h \
  nbservice.h \
  nbspawn.h \
  nbspine.h \
  nbstem.h \
  nbstd.h \
  nbstream.h \
  nbstring.h \
  nbsynapse.h \
  nbterm.h \
  nbtime.h \
  nb-tls.h \
  nbtls.h \
  nbtree.h \
  nbtranslator.h \
  pcre.h \
  readline/chardefs.h \
  readline/history.h \
  readline/keymaps.h \
  readline/readline.h \
  readline/rlconf.h \
  readline/rlstdc.h \
  readline/rltypedefs.h \
  readline/tilde.h

noinst_PROGRAMS = nb

#bin_PROGRAMS = nb

nb_SOURCES = nb.c

#nb_LDADD = .libs/libnb.so
nb_LDADD = .libs/libnb.la

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
