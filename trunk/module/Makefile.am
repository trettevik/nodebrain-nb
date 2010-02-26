
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = audit cache console mail message netflow peer pipe servant snmptrap string syslog toy translator tree udp webster

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
	if test ! -d "@libdir@/nb/$$minor"; then mkdir -p @libdir@/nb/$$minor; fi; \
	ext=`grep LT_MODULE_EXT config.h | sed 's/[^"]*"\([^"]*\)"/\1/'`; \
	major=`echo $$minor | sed 's/\([0-9]*\).[^ ]*/\1/'`; \
	list="$(SUBDIRS)"; \
	for subdir in $$list; do \
	  module=`ls -al $$subdir/.libs/nb_$$subdir$$ext | sed 's/.*nb_[^ ]*.*\(nb\_.*\)/\1/'`; \
	  test -h "@libdir@/nb/$$minor/nb_$$subdir$$ext.$$major" && rm @libdir@/nb/$$minor/nb_$$subdir$$ext.$$major; \
	  ln -s ../$$module @libdir@/nb/$$minor/nb_$$subdir$$ext.$$major; \
	  done

uninstall-local:
	rm -fr $(libdir)/nb/$(VERSION)
