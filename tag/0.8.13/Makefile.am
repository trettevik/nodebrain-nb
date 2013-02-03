## Process this file with automake to produce Makefile.in
## =============================================================================
##
## Copyright (C) 1998-2013 The Boeing Company
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
## Title:    NodeBrain automake input file
##
## Function:
##
##   This file is processed by automake to create the Makefile.in file when
##   a NodeBrain developer is creating a distribution.
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
## 2004/03/25 Ed Trettevik (version 0.6.0)
## 2005/01/06 eat 0.6.2  nbsynapse.c/h, nbstream.c/h and nb_console.c  included
## 2005/03/24 eat 0.6.2  nb_netflow.c and nb_snmptrap.c included
## 2005/03/26 eat 0.6.2  nbip.h and nbip.c included
## 2005/04/02 eat 0.6.2  nb_syslog.c included
## 2005/04/04 eat 0.6.2  nb_translate.c included
## 2005/04/09 eat 0.6.2  experimenting with nb_cache
## 2005/04/20 eat 0.6.2  nb_snmptrapv2.c included
## 2005/05/06 eat 0.6.2  nb_snmptrapv2.c removed
## 2005/12/31 eat 0.6.4  nb_servant.c included
## 2006/05/26 eat 0.6.6  nb_junction.c included
## 2006/05/29 eat 0.6.6  nb_duplicator.c included
## 2007/05/22 eat 0.6.8  nbtree.c/h included
## 2007/05/30 eat 0.6.8  nb_cache no longer internal to nb
## 2007/06/23 eat 0.6.8  nb_peer included
## 2007/06/23 eat 0.6.8  nb_mail included
## 2007/06/23 eat 0.6.8  nb_pipe included
## 2007/06/25 eat 0.6.8  nb_audit included
## 2007/06/26 eat 0.6.8  nb_translate renamed to nb_translator
## 2007/10/21 eat 0.6.8  nb_webster included
## 2008/06/26 eat 0.7.1  Modified to install as "nodebrain" and rename as nb-$(VERSION)
## 2008/10/17 eat 0.7.2  Modified for LIBTOOL
## 2008/10/24 eat 0.7.3  Moving modules down to a sub-package in "module" directory
## 2010/02/27 eat 0.7.9  Included DESTDIR
##=============================================================================

SUBDIRS = lib . module

info_TEXINFOS = doc/nodebrain.texi doc/nbTutorial/nbTutorial.texi doc/nbLanguage/nbLanguage.texi
doc_nbTutorial_TEXINFOS = doc/fdl.texi
doc_nbLanguage_TEXINFOS = doc/fdl.texi

dist_man_MANS = \
  man/caboodle.nb.5 \
  man/NodeBrain.1 \
  man/nb.1 \
  man/nb.3 \
  man/nb.5 \
  man/nb.7 \
  man/nb.cfg.5 \
  man/nb.lib.3 \
  man/nb.man.7 \
  man/nb.mod.7 \
  man/nb.syn.5 \
  man/nodebrain.1 \
  man/user.nb.5

# Build html pages for man pages

html/nb.html: man/nb.1
	bin/nbman2ht man/nb.1 > html/nb.html
html/nb.cfg.html: man/nb.cfg.5
	bin/nbman2ht man/nb.cfg.5 > html/nb.cfg.html
html/nb.lib.html: man/nb.lib.3
	bin/nbman2ht man/nb.lib.3 > html/nb.lib.html
html/nb.man.html: man/nb.man.7
	bin/nbman2ht man/nb.man.7 > html/nb.man.html
html/nb.mod.html: man/nb.mod.7
	bin/nbman2ht man/nb.mod.7 > html/nb.mod.html
html/nb.syn.html: man/nb.syn.5
	bin/nbman2ht man/nb.syn.5 > html/nb.syn.html

noinst_DATA = \
  html/nb.html \
  html/nb.cfg.html \
  html/nb.lib.html \
  html/nb.man.html \
  html/nb.mod.html \
  html/nb.syn.html

##NB_MOD_SUFFIX = so

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = nb.pc

EXTRA_DIST = \
  LICENSE.txt \
  Readme.txt \
  nodebrain.spec.in \
  nodebrain.spec \
  nb.pc.in \
  build-nb \
  install-nb \
  bin/README \
  bin/nbfarm \
  bin/nbman2ht \
  bin/nbpen \
  sample/main.c \
  sample/main.nb \
  sample/snmptrap.log \
  sample/snmptrap.nb \
  sample/snmptrap.sh \
  sample/toyEvents.nb \
  sample/toyRules.nb \
  sample/treeEvents.nb \
  sample/treeRules.nb \
  html/nb.cfg.html \
  html/nb.html \
  html/nb.lib.html \
  html/nb.man.html \
  html/nb.mod.html \
  html/nb.syn.html 

include tutmake.inc

bin_PROGRAMS = nodebrain

nodebrain:
	if test -f lib/.libs/nb; then cp lib/.libs/nb nodebrain; else cp lib/nb nodebrain; fi 
	if test -f nb; then rm -f nb; fi
	ln -s lib/nb nb

nodebrain_SOURCES = 

## Rename nodebrain to nb-x.x.x and create symbolic link from nb if not already defined

install-exec-hook:
	cd ${DESTDIR}@bindir@ && mv -f nodebrain nb-$(VERSION)$(EXEEXT)
	cd ${DESTDIR}@bindir@ && if test ! -f nb -a ! -h nb; then ln -s nb-$(VERSION)$(EXEEXT) nb; fi

## Remove $(bindir)/nb-x.x.x program and $(libdir)/nb/x.x.x directory

uninstall-local:
	rm @bindir@/nb-$(VERSION)$(EXEEXT)

## Run a set of tests to check out a build

#.PHONY: check
#check: all
#	cd caboodle; LD_LIBRARY_PATH=../lib/.libs bin/nb -b check/build.nb
#	cd caboodle; LD_LIBRARY_PATH=../lib/.libs bin/nbcheck

## Create a compressed archive of binary files (not a distribution file)
.PHONY: archive
archive: check
	tar -chof ${distdir}-binary.tar nb

## Create a binary distribution and test it
.PHONY: bindist
bindist: check dist archive
	gunzip -c ${distdir}.tar.gz | tar -xf -
	cd ${distdir}; tar -xf ../${distdir}-binary.tar
	rm ${distdir}.tar.gz ${distdir}-binary.tar
	tar -chof ${distdir}-bindist.tar ${distdir}
	rm -fr ${distdir}
	tar -xf ${distdir}-bindist.tar
	cd ${distdir}; ./nb -b check/build.nb
	cd ${distdir}; bin/nbcheck check
	rm -fr ${distdir}
	gzip ${distdir}-bindist.tar

## Create a source distribution and test it by creating a binary distribution

.PHONY: srcdist
srcdist: check dist
	gunzip -c ${distdir}.tar.gz | tar -xf -
	cd ${distdir}; ./configure
	cd ${distdir}; ${MAKE} bindist
	rm -fr ${distdir}
	mv ${distdir}.tar.gz ${distdir}-source.tar.gz

.PHONY: rpm
rpm: dist
	rpmbuild -ta @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz

