# $Id: dist.mk,v 1.172 1999/10/23 12:29:39 tom Exp $
# Makefile for creating ncurses distributions.
#
# This only needs to be used directly as a makefile by developers, but
# configure mines the current version number out of here.  To move
# to a new version number, just edit this file and run configure.
#
SHELL = /bin/sh

# These define the major/minor/patch versions of ncurses.
NCURSES_MAJOR = 5
NCURSES_MINOR = 0
NCURSES_PATCH = 19991023

# We don't append the patch to the version, since this only applies to releases
VERSION = $(NCURSES_MAJOR).$(NCURSES_MINOR)

DUMP	= lynx -dump
DUMP2	= $(DUMP) -nolist

ALL	= ANNOUNCE announce.html misc/ncurses-intro.doc misc/hackguide.doc

all :	$(ALL)

dist:	$(ALL)
	(cd ..;  tar cvf ncurses-$(VERSION).tar `sed <ncurses-$(VERSION)/MANIFEST 's/^./ncurses-$(VERSION)/'`;  gzip ncurses-$(VERSION).tar)

distclean:
	rm -f $(ALL)

# Don't mess with announce.html.in unless you have lynx available!
announce.html: announce.html.in
	sed 's,@VERSION@,$(VERSION),' <announce.html.in >announce.html

ANNOUNCE : announce.html
	$(DUMP) announce.html >ANNOUNCE

misc/ncurses-intro.doc: misc/ncurses-intro.html
	$(DUMP2) misc/ncurses-intro.html > misc/ncurses-intro.doc
misc/hackguide.doc: misc/hackguide.html
	$(DUMP2) misc/hackguide.html > misc/hackguide.doc

# Prepare distribution for version control
vcprepare:
	find . -type d -exec mkdir {}/RCS \;

# Write-lock almost all files not under version control.
ADA_EXCEPTIONS=$(shell eval 'a="\\\\\|";for x in Ada95/gen/terminal*.m4; do echo -n $${a}Ada95/ada_include/`basename $${x} .m4`; done')
EXCEPTIONS = 'announce.html$\\|ANNOUNCE\\|misc/.*\\.doc\\|man/terminfo.5\\|lib_gen.c'$(ADA_EXCEPTIONS)
writelock:
	for x in `grep -v $(EXCEPTIONS) MANIFEST`; do if [ ! -f `dirname $$x`/RCS/`basename $$x`,v ]; then chmod a-w $${x}; fi; done

# This only works on a clean source tree, of course.
MANIFEST:
	-rm -f $@
	touch $@
	find . -type f -print |sort | fgrep -v .lsm |fgrep -v .spec >$@

TAGS:
	etags */*.[ch]

# Makefile ends here
