.PHONY: 	 clean install installwww
.SUFFIXES:	 .sgml .html .md5 .h .h.html
.SUFFIXES:	 .1       .3       .7       .8
.SUFFIXES:	 .1.html  .3.html  .7.html  .8.html

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
#
# CFLAGS	+= -DOSNAME="\"OpenBSD 5.4\""

VERSION		 = 1.12.3
VDATE		 = 31 December 2013

# IFF your system supports multi-byte functions (setlocale(), wcwidth(),
# putwchar()) AND has __STDC_ISO_10646__ (that is, wchar_t is simply a
# UCS-4 value) should you define USE_WCHAR.  If you define it and your
# system DOESN'T support this, -Tlocale will produce garbage.
# If you don't define it, -Tlocale is a synonym for -Tacsii.
#
CFLAGS	 	+= -DUSE_WCHAR

# If your system has manpath(1), uncomment this.  This is most any
# system that's not OpenBSD or NetBSD.  If uncommented, apropos(1),
# mandocdb(8), and man.cgi will popen(3) manpath(1) to get the MANPATH
# variable.
#CFLAGS		+= -DUSE_MANPATH

# If your system does not support static binaries, comment this,
# for example on Mac OS X.
STATIC		 = -static
# Linux requires -pthread to statically link with libdb.
#STATIC		+= -pthread

CFLAGS		+= -g -DHAVE_CONFIG_H
CFLAGS     	+= -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
PREFIX		 = /usr/local
WWWPREFIX	 = /var/www
HTDOCDIR	 = $(WWWPREFIX)/htdocs
CGIBINDIR	 = $(WWWPREFIX)/cgi-bin
BINDIR		 = $(PREFIX)/bin
INCLUDEDIR	 = $(PREFIX)/include/mandoc
LIBDIR		 = $(PREFIX)/lib/mandoc
MANDIR		 = $(PREFIX)/man
EXAMPLEDIR	 = $(PREFIX)/share/examples/mandoc
INSTALL		 = install
INSTALL_PROGRAM	 = $(INSTALL) -m 0755
INSTALL_DATA	 = $(INSTALL) -m 0444
INSTALL_LIB	 = $(INSTALL) -m 0644
INSTALL_SOURCE	 = $(INSTALL) -m 0644
INSTALL_MAN	 = $(INSTALL_DATA)

# Non-BSD systems (Linux, etc.) need -ldb to compile mandocdb and
# apropos.
# However, if you don't have -ldb at all (or it's not native), then
# comment out apropos and mandocdb. 
#
#DBLIB		 = -ldb
DBBIN		 = apropos mandocdb man.cgi catman whatis
DBLN		 = llib-lapropos.ln llib-lmandocdb.ln llib-lman.cgi.ln llib-lcatman.ln

all: mandoc preconv demandoc $(DBBIN)

SRCS		 = Makefile \
		   NEWS \
		   TODO \
		   apropos.1 \
		   apropos.c \
		   apropos_db.c \
		   apropos_db.h \
		   arch.c \
		   arch.in \
		   att.c \
		   att.in \
		   catman.8 \
		   catman.c \
		   cgi.c \
		   chars.c \
		   chars.in \
		   compat_fgetln.c \
		   compat_getsubopt.c \
		   compat_strlcat.c \
		   compat_strlcpy.c \
		   config.h.post \
		   config.h.pre \
		   demandoc.1 \
		   demandoc.c \
		   eqn.7 \
		   eqn.c \
		   eqn_html.c \
		   eqn_term.c \
		   example.style.css \
		   external.png \
		   gmdiff \
		   html.c \
		   html.h \
		   index.css \
		   index.sgml \
		   lib.c \
		   lib.in \
		   libman.h \
		   libmandoc.h \
		   libmdoc.h \
		   libroff.h \
		   main.c \
		   main.h \
		   man.7 \
		   man.c \
		   man.cgi.7 \
		   man-cgi.css \
		   man.h \
		   man_hash.c \
		   man_html.c \
		   man_macro.c \
		   man_term.c \
		   man_validate.c \
		   mandoc.1 \
		   mandoc.3 \
		   mandoc.c \
		   mandoc.h \
		   mandoc_char.7 \
		   mandocdb.8 \
		   mandocdb.c \
		   mandocdb.h \
		   manpath.c \
		   manpath.h \
		   mdoc.7 \
		   mdoc.c \
		   mdoc.h \
		   mdoc_argv.c \
		   mdoc_hash.c \
		   mdoc_html.c \
		   mdoc_macro.c \
		   mdoc_man.c \
		   mdoc_term.c \
		   mdoc_validate.c \
		   msec.c \
		   msec.in \
		   out.c \
		   out.h \
		   preconv.1 \
		   preconv.c \
		   predefs.in \
		   read.c \
		   roff.7 \
		   roff.c \
		   st.c \
		   st.in \
		   style.css \
		   tbl.3 \
		   tbl.7 \
		   tbl.c \
		   tbl_data.c \
		   tbl_html.c \
		   tbl_layout.c \
		   tbl_opts.c \
		   tbl_term.c \
		   term.c \
		   term.h \
		   term_ascii.c \
		   term_ps.c \
		   test-betoh64.c \
		   test-fgetln.c \
		   test-getsubopt.c \
		   test-mmap.c \
		   test-strlcat.c \
		   test-strlcpy.c \
		   test-strptime.c \
		   tree.c \
		   vol.c \
		   vol.in \
		   whatis.1

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
		   msec.o \
		   read.o

COMPAT_OBJS	 = compat_fgetln.o \
		   compat_getsubopt.o \
		   compat_strlcat.o \
		   compat_strlcpy.o

arch.o: arch.in
att.o: att.in
chars.o: chars.in
lib.o: lib.in
msec.o: msec.in
roff.o: predefs.in
st.o: st.in
vol.o: vol.in

$(LIBMAN_OBJS): libman.h
$(LIBMDOC_OBJS): libmdoc.h
$(LIBROFF_OBJS): libroff.h
$(LIBMANDOC_OBJS): mandoc.h mdoc.h man.h libmandoc.h config.h
$(COMPAT_OBJS): config.h

MANDOC_HTML_OBJS = eqn_html.o \
		   html.o \
		   man_html.o \
		   mdoc_html.o \
		   tbl_html.o
$(MANDOC_HTML_OBJS): html.h

MANDOC_MAN_OBJS  = mdoc_man.o

MANDOC_TERM_OBJS = eqn_term.o \
		   man_term.o \
		   mdoc_term.o \
		   term.o \
		   term_ascii.o \
		   term_ps.o \
		   tbl_term.o
$(MANDOC_TERM_OBJS): term.h

MANDOC_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   main.o \
		   out.o \
		   tree.o
$(MANDOC_OBJS): main.h mandoc.h mdoc.h man.h config.h out.h

MANDOCDB_OBJS	 = mandocdb.o manpath.o
$(MANDOCDB_OBJS): mandocdb.h mandoc.h mdoc.h man.h config.h manpath.h

PRECONV_OBJS	 = preconv.o
$(PRECONV_OBJS): config.h

APROPOS_OBJS	 = apropos.o apropos_db.o manpath.o
$(APROPOS_OBJS): config.h mandoc.h apropos_db.h manpath.h mandocdb.h

CGI_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   cgi.o \
		   apropos_db.o \
		   manpath.o \
		   out.o \
		   tree.o
$(CGI_OBJS): main.h mdoc.h man.h out.h config.h mandoc.h apropos_db.h manpath.h mandocdb.h

CATMAN_OBJS	 = catman.o manpath.o
$(CATMAN_OBJS): config.h mandoc.h manpath.h mandocdb.h

DEMANDOC_OBJS	 = demandoc.o
$(DEMANDOC_OBJS): config.h

INDEX_MANS	 = apropos.1.html \
		   catman.8.html \
		   demandoc.1.html \
		   mandoc.1.html \
		   whatis.1.html \
		   mandoc.3.html \
		   tbl.3.html \
		   eqn.7.html \
		   man.7.html \
		   man.cgi.7.html \
		   mandoc_char.7.html \
		   mdoc.7.html \
		   preconv.1.html \
		   roff.7.html \
		   tbl.7.html \
		   mandocdb.8.html

$(INDEX_MANS): mandoc

INDEX_OBJS	 = $(INDEX_MANS) \
		   man.h.html \
		   mandoc.h.html \
		   mdoc.h.html \
		   mdocml.tar.gz \
		   mdocml.md5

www: index.html

clean:
	rm -f libmandoc.a $(LIBMANDOC_OBJS)
	rm -f mandocdb $(MANDOCDB_OBJS)
	rm -f preconv $(PRECONV_OBJS)
	rm -f apropos whatis $(APROPOS_OBJS)
	rm -f man.cgi $(CGI_OBJS)
	rm -f catman $(CATMAN_OBJS)
	rm -f demandoc $(DEMANDOC_OBJS)
	rm -f mandoc $(MANDOC_OBJS)
	rm -f config.h config.log $(COMPAT_OBJS)
	rm -f mdocml.tar.gz
	rm -f index.html $(INDEX_OBJS)
	rm -rf *.dSYM

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_PROGRAM) mandoc preconv demandoc $(DESTDIR)$(BINDIR)
	$(INSTALL_LIB) libmandoc.a $(DESTDIR)$(LIBDIR)
	$(INSTALL_LIB) man.h mdoc.h mandoc.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_MAN) mandoc.1 preconv.1 demandoc.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) mandoc.3 tbl.3 $(DESTDIR)$(MANDIR)/man3
	$(INSTALL_MAN) man.7 mdoc.7 roff.7 eqn.7 tbl.7 mandoc_char.7 $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(EXAMPLEDIR)

installcgi: all
	mkdir -p $(DESTDIR)$(CGIBINDIR)
	mkdir -p $(DESTDIR)$(HTDOCDIR)
	$(INSTALL_PROGRAM) man.cgi $(DESTDIR)$(CGIBINDIR)
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(HTDOCDIR)/man.css
	$(INSTALL_DATA) man-cgi.css $(DESTDIR)$(HTDOCDIR)

installwww: www
	mkdir -p $(PREFIX)/snapshots
	mkdir -p $(PREFIX)/binaries
	$(INSTALL_DATA) index.html external.png index.css $(PREFIX)
	$(INSTALL_DATA) $(INDEX_MANS) style.css $(PREFIX)
	$(INSTALL_DATA) mandoc.h.html man.h.html mdoc.h.html $(PREFIX)
	$(INSTALL_DATA) mdocml.tar.gz $(PREFIX)/snapshots
	$(INSTALL_DATA) mdocml.md5 $(PREFIX)/snapshots
	$(INSTALL_DATA) mdocml.tar.gz $(PREFIX)/snapshots/mdocml-$(VERSION).tar.gz
	$(INSTALL_DATA) mdocml.md5 $(PREFIX)/snapshots/mdocml-$(VERSION).md5

libmandoc.a: $(COMPAT_OBJS) $(LIBMANDOC_OBJS)
	$(AR) rs $@ $(COMPAT_OBJS) $(LIBMANDOC_OBJS)

mandoc: $(MANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOC_OBJS) libmandoc.a

mandocdb: $(MANDOCDB_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOCDB_OBJS) libmandoc.a $(DBLIB)

preconv: $(PRECONV_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(PRECONV_OBJS)

whatis: apropos
	cp -f apropos whatis

apropos: $(APROPOS_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(APROPOS_OBJS) libmandoc.a $(DBLIB)

catman: $(CATMAN_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(CATMAN_OBJS) libmandoc.a $(DBLIB)

man.cgi: $(CGI_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) $(STATIC) -o $@ $(CGI_OBJS) libmandoc.a $(DBLIB)

demandoc: $(DEMANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(DEMANDOC_OBJS) libmandoc.a

mdocml.md5: mdocml.tar.gz
	md5 mdocml.tar.gz >$@

mdocml.tar.gz: $(SRCS)
	mkdir -p .dist/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .dist/mdocml-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

index.html: $(INDEX_OBJS)

config.h: config.h.pre config.h.post
	rm -f config.log
	( cat config.h.pre; \
	  echo; \
	  echo '#define VERSION "$(VERSION)"'; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-fgetln test-fgetln.c >> config.log 2>&1; then \
		echo '#define HAVE_FGETLN'; \
		rm test-fgetln; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-strptime test-strptime.c >> config.log 2>&1; then \
		echo '#define HAVE_STRPTIME'; \
		rm test-strptime; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-getsubopt test-getsubopt.c >> config.log 2>&1; then \
		echo '#define HAVE_GETSUBOPT'; \
		rm test-getsubopt; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-strlcat test-strlcat.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCAT'; \
		rm test-strlcat; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-mmap test-mmap.c >> config.log 2>&1; then \
		echo '#define HAVE_MMAP'; \
		rm test-mmap; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-strlcpy test-strlcpy.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCPY'; \
		rm test-strlcpy; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -Wno-unused -o test-betoh64 test-betoh64.c >> config.log 2>&1; then \
		echo '#define HAVE_BETOH64'; \
		rm test-betoh64; \
	  fi; \
	  echo; \
	  cat config.h.post \
	) > $@

.h.h.html:
	highlight -I $< >$@

.1.1.html .3.3.html .7.7.html .8.8.html:
	./mandoc -Thtml -Wall,stop -Ostyle=style.css,man=%N.%S.html,includes=%I.html $< >$@

.sgml.html:
	validate --warn $<
	sed -e "s!@VERSION@!$(VERSION)!" -e "s!@VDATE@!$(VDATE)!" $< >$@
