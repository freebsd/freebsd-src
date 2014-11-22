# $Id: Makefile,v 1.435 2014/08/10 02:45:04 schwarze Exp $
#
# Copyright (c) 2010, 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
# Copyright (c) 2011, 2013, 2014 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

VERSION		 = 1.13.1

# === USER SETTINGS ====================================================

# --- user settings relevant for all builds ----------------------------

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
#
# CFLAGS	+= -DOSNAME="\"OpenBSD 5.5\""

# IFF your system supports multi-byte functions (setlocale(), wcwidth(),
# putwchar()) AND has __STDC_ISO_10646__ (that is, wchar_t is simply a
# UCS-4 value) should you define USE_WCHAR.  If you define it and your
# system DOESN'T support this, -Tlocale will produce garbage.
# If you don't define it, -Tlocale is a synonym for -Tacsii.
#
CFLAGS	 	+= -DUSE_WCHAR

CFLAGS		+= -g -DHAVE_CONFIG_H
CFLAGS     	+= -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
PREFIX		 = /usr/local
BINDIR		 = $(PREFIX)/bin
INCLUDEDIR	 = $(PREFIX)/include/mandoc
LIBDIR		 = $(PREFIX)/lib/mandoc
MANDIR		 = $(PREFIX)/man
EXAMPLEDIR	 = $(PREFIX)/share/examples/mandoc

INSTALL		 = install
INSTALL_PROGRAM	 = $(INSTALL) -m 0555
INSTALL_DATA	 = $(INSTALL) -m 0444
INSTALL_LIB	 = $(INSTALL) -m 0444
INSTALL_SOURCE	 = $(INSTALL) -m 0644
INSTALL_MAN	 = $(INSTALL_DATA)

# --- user settings related to database support ------------------------

# Building apropos(1) and makewhatis(8) requires both SQLite3 and fts(3).
# To avoid those dependencies, comment the following line.
# Be careful: the fts(3) implementation in glibc is broken on 32bit
# machines, see: https://sourceware.org/bugzilla/show_bug.cgi?id=15838
#
BUILD_TARGETS	+= db-build

# The remaining settings in this section
# are only relevant if db-build is enabled.
# Otherwise, they have no effect either way.

# If your system has manpath(1), uncomment this.  This is most any
# system that's not OpenBSD or NetBSD.  If uncommented, apropos(1)
# and makewhatis(8) will use manpath(1) to get the MANPATH variable.
#
#CFLAGS		+= -DUSE_MANPATH

# On some systems, SQLite3 may be installed below /usr/local.
# In that case, uncomment the following two lines.
#
#CFLAGS		+= -I/usr/local/include
#DBLIB		+= -L/usr/local/lib

# OpenBSD has the ohash functions in libutil.
# Comment the following line if your system doesn't.
#
DBLIB		+= -lutil

SBINDIR		 = $(PREFIX)/sbin

# --- user settings related to man.cgi ---------------------------------

# To build man.cgi, copy cgi.h.example to cgi.h, edit it,
# and enable the following line.
# Obviously, this requires that db-build is enabled, too.
#
#BUILD_TARGETS	+= cgi-build

# The remaining settings in this section
# are only relevant if cgi-build is enabled.
# Otherwise, they have no effect either way.

# If your system does not support static binaries, comment this,
# for example on Mac OS X.
#
STATIC		 = -static

# Linux requires -pthread for statical linking.
#
#STATIC		+= -pthread

WWWPREFIX	 = /var/www
HTDOCDIR	 = $(WWWPREFIX)/htdocs
CGIBINDIR	 = $(WWWPREFIX)/cgi-bin

# === END OF USER SETTINGS =============================================

INSTALL_TARGETS	 = $(BUILD_TARGETS:-build=-install)

BASEBIN		 = mandoc preconv demandoc
DBBIN		 = apropos makewhatis
CGIBIN		 = man.cgi

DBLIB		+= -lsqlite3

TESTSRCS	 = test-fgetln.c \
		   test-getsubopt.c \
		   test-mmap.c \
		   test-ohash.c \
		   test-reallocarray.c \
		   test-sqlite3_errstr.c \
		   test-strcasestr.c \
		   test-strlcat.c \
		   test-strlcpy.c \
		   test-strptime.c \
		   test-strsep.c

SRCS		 = apropos.c \
		   arch.c \
		   att.c \
		   cgi.c \
		   chars.c \
		   compat_fgetln.c \
		   compat_getsubopt.c \
		   compat_ohash.c \
		   compat_reallocarray.c \
		   compat_sqlite3_errstr.c \
		   compat_strcasestr.c \
		   compat_strlcat.c \
		   compat_strlcpy.c \
		   compat_strsep.c \
		   demandoc.c \
		   eqn.c \
		   eqn_html.c \
		   eqn_term.c \
		   html.c \
		   lib.c \
		   main.c \
		   man.c \
		   man_hash.c \
		   man_html.c \
		   man_macro.c \
		   man_term.c \
		   man_validate.c \
		   mandoc.c \
		   mandoc_aux.c \
		   mandocdb.c \
		   manpage.c \
		   manpath.c \
		   mansearch.c \
		   mansearch_const.c \
		   mdoc.c \
		   mdoc_argv.c \
		   mdoc_hash.c \
		   mdoc_html.c \
		   mdoc_macro.c \
		   mdoc_man.c \
		   mdoc_term.c \
		   mdoc_validate.c \
		   msec.c \
		   out.c \
		   preconv.c \
		   read.c \
		   roff.c \
		   st.c \
		   tbl.c \
		   tbl_data.c \
		   tbl_html.c \
		   tbl_layout.c \
		   tbl_opts.c \
		   tbl_term.c \
		   term.c \
		   term_ascii.c \
		   term_ps.c \
		   tree.c \
		   vol.c \
		   $(TESTSRCS)

DISTFILES	 = INSTALL \
		   LICENSE \
		   Makefile \
		   Makefile.depend \
		   NEWS \
		   TODO \
		   apropos.1 \
		   arch.in \
		   att.in \
		   cgi.h.example \
		   chars.in \
		   compat_ohash.h \
		   config.h.post \
		   config.h.pre \
		   configure \
		   demandoc.1 \
		   eqn.7 \
		   example.style.css \
		   gmdiff \
		   html.h \
		   lib.in \
		   libman.h \
		   libmandoc.h \
		   libmdoc.h \
		   libroff.h \
		   main.h \
		   makewhatis.8 \
		   man-cgi.css \
		   man.7 \
		   man.cgi.8 \
		   man.h \
		   mandoc.1 \
		   mandoc.3 \
		   mandoc.db.5 \
		   mandoc.h \
		   mandoc_aux.h \
		   mandoc_char.7 \
		   mandoc_escape.3 \
		   mandoc_html.3 \
		   mandoc_malloc.3 \
		   manpath.h \
		   mansearch.3 \
		   mansearch.h \
		   mchars_alloc.3 \
		   mdoc.7 \
		   mdoc.h \
		   msec.in \
		   out.h \
		   preconv.1 \
		   predefs.in \
		   roff.7 \
		   st.in \
		   style.css \
		   tbl.3 \
		   tbl.7 \
		   term.h \
		   vol.in \
		   $(SRCS)

LIBMAN_OBJS	 = man.o \
		   man_hash.o \
		   man_macro.o \
		   man_validate.o

LIBMDOC_OBJS	 = arch.o \
		   att.o \
		   lib.o \
		   mdoc.o \
		   mdoc_argv.o \
		   mdoc_hash.o \
		   mdoc_macro.o \
		   mdoc_validate.o \
		   st.o \
		   vol.o

LIBROFF_OBJS	 = eqn.o \
		   roff.o \
		   tbl.o \
		   tbl_data.o \
		   tbl_layout.o \
		   tbl_opts.o

LIBMANDOC_OBJS	 = $(LIBMAN_OBJS) \
		   $(LIBMDOC_OBJS) \
		   $(LIBROFF_OBJS) \
		   chars.o \
		   mandoc.o \
		   mandoc_aux.o \
		   msec.o \
		   read.o

COMPAT_OBJS	 = compat_fgetln.o \
		   compat_getsubopt.o \
		   compat_ohash.o \
		   compat_reallocarray.o \
		   compat_sqlite3_errstr.o \
		   compat_strcasestr.o \
		   compat_strlcat.o \
		   compat_strlcpy.o \
		   compat_strsep.o

MANDOC_HTML_OBJS = eqn_html.o \
		   html.o \
		   man_html.o \
		   mdoc_html.o \
		   tbl_html.o

MANDOC_MAN_OBJS  = mdoc_man.o

MANDOC_TERM_OBJS = eqn_term.o \
		   man_term.o \
		   mdoc_term.o \
		   term.o \
		   term_ascii.o \
		   term_ps.o \
		   tbl_term.o

MANDOC_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   main.o \
		   out.o \
		   tree.o

MAKEWHATIS_OBJS	 = mandocdb.o mansearch_const.o manpath.o

PRECONV_OBJS	 = preconv.o

APROPOS_OBJS	 = apropos.o mansearch.o mansearch_const.o manpath.o

CGI_OBJS	 = $(MANDOC_HTML_OBJS) \
		   cgi.o \
		   mansearch.o \
		   mansearch_const.o \
		   out.o

MANPAGE_OBJS	 = manpage.o mansearch.o mansearch_const.o manpath.o

DEMANDOC_OBJS	 = demandoc.o

WWW_MANS	 = apropos.1.html \
		   demandoc.1.html \
		   mandoc.1.html \
		   preconv.1.html \
		   mandoc.3.html \
		   mandoc_escape.3.html \
		   mandoc_html.3.html \
		   mandoc_malloc.3.html \
		   mansearch.3.html \
		   mchars_alloc.3.html \
		   tbl.3.html \
		   mandoc.db.5.html \
		   eqn.7.html \
		   man.7.html \
		   mandoc_char.7.html \
		   mdoc.7.html \
		   roff.7.html \
		   tbl.7.html \
		   makewhatis.8.html \
		   man.cgi.8.html \
		   man.h.html \
		   mandoc.h.html \
		   mandoc_aux.h.html \
		   manpath.h.html \
		   mansearch.h.html \
		   mdoc.h.html

WWW_OBJS	 = mdocml.tar.gz \
		   mdocml.sha256

# === DEPENDENCY HANDLING ==============================================

all: base-build $(BUILD_TARGETS)

base-build: $(BASEBIN)

db-build: $(DBBIN)

cgi-build: $(CGIBIN)

install: base-install $(INSTALL_TARGETS)

www: $(WWW_OBJS) $(WWW_MANS)

include Makefile.depend

# === TARGETS CONTAINING SHELL COMMANDS ================================

clean:
	rm -f libmandoc.a $(LIBMANDOC_OBJS)
	rm -f apropos $(APROPOS_OBJS)
	rm -f makewhatis $(MAKEWHATIS_OBJS)
	rm -f preconv $(PRECONV_OBJS)
	rm -f man.cgi $(CGI_OBJS)
	rm -f manpage $(MANPAGE_OBJS)
	rm -f demandoc $(DEMANDOC_OBJS)
	rm -f mandoc $(MANDOC_OBJS)
	rm -f config.h config.log $(COMPAT_OBJS)
	rm -f $(WWW_MANS) $(WWW_OBJS)
	rm -rf *.dSYM

base-install: base-build
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_PROGRAM) $(BASEBIN) $(DESTDIR)$(BINDIR)
	$(INSTALL_LIB) libmandoc.a $(DESTDIR)$(LIBDIR)
	$(INSTALL_LIB) man.h mandoc.h mandoc_aux.h mdoc.h \
		$(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_MAN) mandoc.1 preconv.1 demandoc.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) mandoc.3 mandoc_escape.3 mandoc_malloc.3 \
		mchars_alloc.3 tbl.3 $(DESTDIR)$(MANDIR)/man3
	$(INSTALL_MAN) man.7 mdoc.7 roff.7 eqn.7 tbl.7 mandoc_char.7 \
		$(DESTDIR)$(MANDIR)/man7
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(EXAMPLEDIR)

db-install: db-build
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(SBINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p $(DESTDIR)$(MANDIR)/man8
	$(INSTALL_PROGRAM) apropos $(DESTDIR)$(BINDIR)
	ln -f $(DESTDIR)$(BINDIR)/apropos $(DESTDIR)$(BINDIR)/whatis
	$(INSTALL_PROGRAM) makewhatis $(DESTDIR)$(SBINDIR)
	$(INSTALL_MAN) apropos.1 $(DESTDIR)$(MANDIR)/man1
	ln -f $(DESTDIR)$(MANDIR)/man1/apropos.1 \
		$(DESTDIR)$(MANDIR)/man1/whatis.1
	$(INSTALL_MAN) mansearch.3 $(DESTDIR)$(MANDIR)/man3
	$(INSTALL_MAN) mandoc.db.5 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_MAN) makewhatis.8 $(DESTDIR)$(MANDIR)/man8

cgi-install: cgi-build
	mkdir -p $(DESTDIR)$(CGIBINDIR)
	mkdir -p $(DESTDIR)$(HTDOCDIR)
	mkdir -p $(DESTDIR)$(WWWPREFIX)/man/mandoc/man1
	mkdir -p $(DESTDIR)$(WWWPREFIX)/man/mandoc/man8
	$(INSTALL_PROGRAM) man.cgi $(DESTDIR)$(CGIBINDIR)
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(HTDOCDIR)/man.css
	$(INSTALL_DATA) man-cgi.css $(DESTDIR)$(HTDOCDIR)
	$(INSTALL_MAN) apropos.1 $(DESTDIR)$(WWWPREFIX)/man/mandoc/man1/
	$(INSTALL_MAN) man.cgi.8 $(DESTDIR)$(WWWPREFIX)/man/mandoc/man8/

www-install: www
	mkdir -p $(DESTDIR)$(HTDOCDIR)/snapshots
	$(INSTALL_DATA) $(WWW_MANS) style.css $(DESTDIR)$(HTDOCDIR)
	$(INSTALL_DATA) $(WWW_OBJS) $(DESTDIR)$(HTDOCDIR)/snapshots
	$(INSTALL_DATA) mdocml.tar.gz \
		$(DESTDIR)$(HTDOCDIR)/snapshots/mdocml-$(VERSION).tar.gz
	$(INSTALL_DATA) mdocml.sha256 \
		$(DESTDIR)$(HTDOCDIR)/snapshots/mdocml-$(VERSION).sha256

depend: config.h
	mkdep -f Makefile.depend $(CFLAGS) $(SRCS)
	perl -e 'undef $$/; $$_ = <>; s|/usr/include/\S+||g; \
		s|\\\n||g; s|  +| |g; print;' Makefile.depend > Makefile.tmp
	mv Makefile.tmp Makefile.depend

libmandoc.a: $(COMPAT_OBJS) $(LIBMANDOC_OBJS)
	$(AR) rs $@ $(COMPAT_OBJS) $(LIBMANDOC_OBJS)

mandoc: $(MANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOC_OBJS) libmandoc.a

makewhatis: $(MAKEWHATIS_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MAKEWHATIS_OBJS) libmandoc.a $(DBLIB)

preconv: $(PRECONV_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(PRECONV_OBJS)

manpage: $(MANPAGE_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANPAGE_OBJS) libmandoc.a $(DBLIB)

apropos: $(APROPOS_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(APROPOS_OBJS) libmandoc.a $(DBLIB)

man.cgi: $(CGI_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) $(STATIC) -o $@ $(CGI_OBJS) libmandoc.a $(DBLIB)

demandoc: $(DEMANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(DEMANDOC_OBJS) libmandoc.a

mdocml.sha256: mdocml.tar.gz
	sha256 mdocml.tar.gz > $@

mdocml.tar.gz: $(DISTFILES)
	mkdir -p .dist/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(DISTFILES) .dist/mdocml-$(VERSION)
	chmod 755 .dist/mdocml-$(VERSION)/configure
	( cd .dist/ && tar zcf ../$@ mdocml-$(VERSION) )
	rm -rf .dist/

config.h: configure config.h.pre config.h.post $(TESTSRCS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" DBLIB="$(DBLIB)" \
		VERSION="$(VERSION)" ./configure

.PHONY: 	 base-install cgi-install db-install install www-install
.PHONY: 	 clean depend
.SUFFIXES:	 .1       .3       .5       .7       .8       .h
.SUFFIXES:	 .1.html  .3.html  .5.html  .7.html  .8.html  .h.html

.h.h.html:
	highlight -I $< > $@

.1.1.html .3.3.html .5.5.html .7.7.html .8.8.html: mandoc
	./mandoc -Thtml -Wall,stop \
		-Ostyle=style.css,man=%N.%S.html,includes=%I.html $< > $@
