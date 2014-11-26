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
## 2013/02/03 eat 0.8.13 Included nbkit man pages
## 2014-02-18 eat 0.9.01 Included NodeBrain Library manual
##=============================================================================

ACLOCAL_AMFLAGS= -I m4

SUBDIRS = lib . module

info_TEXINFOS = doc/nodebrain.texi doc/nbGuide/nbGuide.texi doc/nbTutorial/nbTutorial.texi doc/nbLanguage/nbLanguage.texi doc/nbLibrary/nbLibrary.texi
doc_nbGuide_nbGuide_TEXINFOS = \
  doc/nbGuide/makedoc \
  doc/nbGuide/licenses.texi \
  doc/nbGuide/RelationalTermRichRulesAssert.png \
  doc/nbGuide/RelationalTermRichRulesAssert.txt \
  doc/nbGuide/RelationalValueRichRulesAlert.png \
  doc/nbGuide/RelationalValueRichRulesAlert.txt \
  doc/nbGuide/RelationalValueRichRulesAssert.png \
  doc/nbGuide/RelationalValueRichRulesAssert.txt
doc_nbLanguage_nbLanguage_TEXINFOS = \
  doc/nbLanguage/makedoc \
  doc/nbLanguage/style.css \
  doc/nbLanguage/licenses.texi \
  doc/nbLanguage/langref*
doc_nbLibrary_nbLibrary_TEXINFOS = \
  doc/nbLibrary/makedoc \
  doc/nbLibrary/licenses.texi \
  doc/nbLibrary/Interfaces.png \
  doc/nbLibrary/Interfaces.txt
doc_nbTutorial_nbTutorial_TEXINFOS = \
  doc/nbTutorial/makedoc \
  doc/nbTutorial/licenses.texi


dist_man_MANS = \
  man/nb.1 \
  man/nb.cfg.5 \
  man/nb.lib.3 \
  man/nb.mod.7 \
  man/nb.syn.5 \
  man/nbkit.1 \
  man/nbkit.caboodle.7 \
  man/nodebrain.7

# Build html pages for man pages

html/nb.html: man/nb.1
	bin/nbman2ht man/nb.1 > html/nb.html
html/nb.cfg.html: man/nb.cfg.5
	bin/nbman2ht man/nb.cfg.5 > html/nb.cfg.html
html/nb.lib.html: man/nb.lib.3
	bin/nbman2ht man/nb.lib.3 > html/nb.lib.html
html/nb.mod.html: man/nb.mod.7
	bin/nbman2ht man/nb.mod.7 > html/nb.mod.html
html/nb.syn.html: man/nb.syn.5
	bin/nbman2ht man/nb.syn.5 > html/nb.syn.html
html/nbkit.html: man/nbkit.1
	bin/nbman2ht man/nbkit.1 > html/nbkit.html
html/nbkit.caboodle.html: man/nbkit.caboodle.7
	bin/nbman2ht man/nbkit.caboodle.7 > html/nbkit.caboodle.html
html/nodebrain.html: man/nodebrain.7
	bin/nbman2ht man/nodebrain.7 > html/nodebrain.html

noinst_DATA = \
  html/nb.html \
  html/nb.cfg.html \
  html/nb.lib.html \
  html/nb.mod.html \
  html/nb.syn.html \
  html/nbkit.html \
  html/nbkit.caboodle.html \
  html/nodebrain.html

##NB_MOD_SUFFIX = so

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = nb.pc

EXTRA_DIST = \
  LICENSE.txt \
  Readme.txt \
  nodebrain.spec.in \
  nodebrain.spec \
  debian/compat \
  debian/control \
  debian/rules \
  debian/changelog \
  debian/copyright \
  doc/documents.texi \
  doc/licenses.texi \
  doc/style.css \
  nb.pc.in \
  build-nb \
  install-nb \
  bin/README \
  bin/nbchange \
  bin/nbgenperf \
  bin/nbman2ht \
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
  html/nb.mod.html \
  html/nb.syn.html \
  html/nodebrain.html

include tutmake.inc

## bin_PROGRAMS = nodebrain

## nodebrain:
##	if test -f lib/.libs/nb; then cp lib/.libs/nb nodebrain; else cp lib/nb nodebrain; fi 
##	if test -f nb; then rm -f nb; fi
##	ln -s lib/nb nb

## nodebrain_SOURCES = 

## Rename nodebrain to nb-x.x.x and create symbolic link from nb if not already defined

##	cd ${DESTDIR}@bindir@ && mv -f nodebrain nb-$(VERSION)$(EXEEXT)
##	cd ${DESTDIR}@bindir@ && if test ! -f nb -a ! -h nb; then ln -s nb-$(VERSION)$(EXEEXT) nb; fi
install-exec-hook:
	cd ${DESTDIR}@bindir@ && if test ! -f nbkit -a ! -h nbkit; then ln -s nb nbkit; fi

## Remove $(bindir)/nb-x.x.x program and $(libdir)/nb/x.x.x directory

##	rm @bindir@/nb-$(VERSION)$(EXEEXT)
uninstall-local:
	rm @bindir@/nbkit

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

