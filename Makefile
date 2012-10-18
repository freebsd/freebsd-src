.PHONY: 	 clean install installwww
.SUFFIXES:	 .sgml .html .md5 .h .h.html
.SUFFIXES:	 .1       .3       .7       .8
.SUFFIXES:	 .1.txt   .3.txt   .7.txt   .8.txt
.SUFFIXES:	 .1.pdf   .3.pdf   .7.pdf   .8.pdf
.SUFFIXES:	 .1.ps    .3.ps    .7.ps    .8.ps
.SUFFIXES:	 .1.html  .3.html  .7.html  .8.html
.SUFFIXES:	 .1.xhtml .3.xhtml .7.xhtml .8.xhtml

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
#
# CFLAGS	+= -DOSNAME="\"OpenBSD 4.5\""

VERSION		 = 1.12.1
VDATE		 = 23 March 2012

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

# If your system supports static binaries only, uncomment this.  This
# appears only to be BSD UNIX systems (Mac OS X has no support and Linux
# requires -pthreads for static libdb).
STATIC		 = -static

CFLAGS		+= -g -DHAVE_CONFIG_H -DVERSION="\"$(VERSION)\""
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
LIBMAN_LNS	 = man.ln \
		   man_hash.ln \
		   man_macro.ln \
		   man_validate.ln

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
LIBMDOC_LNS	 = arch.ln \
		   att.ln \
		   lib.ln \
		   mdoc.ln \
		   mdoc_argv.ln \
		   mdoc_hash.ln \
		   mdoc_macro.ln \
		   mdoc_validate.ln \
		   st.ln \
		   vol.ln

LIBROFF_OBJS	 = eqn.o \
		   roff.o \
		   tbl.o \
		   tbl_data.o \
		   tbl_layout.o \
		   tbl_opts.o
LIBROFF_LNS	 = eqn.ln \
		   roff.ln \
		   tbl.ln \
		   tbl_data.ln \
		   tbl_layout.ln \
		   tbl_opts.ln

LIBMANDOC_OBJS	 = $(LIBMAN_OBJS) \
		   $(LIBMDOC_OBJS) \
		   $(LIBROFF_OBJS) \
		   chars.o \
		   mandoc.o \
		   msec.o \
		   read.o
LIBMANDOC_LNS	 = $(LIBMAN_LNS) \
		   $(LIBMDOC_LNS) \
		   $(LIBROFF_LNS) \
		   chars.ln \
		   mandoc.ln \
		   msec.ln \
		   read.ln

COMPAT_OBJS	 = compat_fgetln.o \
		   compat_getsubopt.o \
		   compat_strlcat.o \
		   compat_strlcpy.o
COMPAT_LNS	 = compat_fgetln.ln \
		   compat_getsubopt.ln \
		   compat_strlcat.ln \
		   compat_strlcpy.ln

arch.o arch.ln: arch.in
att.o att.ln: att.in
chars.o chars.ln: chars.in
lib.o lib.ln: lib.in
msec.o msec.ln: msec.in
roff.o roff.ln: predefs.in
st.o st.ln: st.in
vol.o vol.ln: vol.in

$(LIBMAN_OBJS) $(LIBMAN_LNS): libman.h
$(LIBMDOC_OBJS) $(LIBMDOC_LNS): libmdoc.h
$(LIBROFF_OBJS) $(LIBROFF_LNS): libroff.h
$(LIBMANDOC_OBJS) $(LIBMANDOC_LNS): mandoc.h mdoc.h man.h libmandoc.h config.h

$(COMPAT_OBJS) $(COMPAT_LNS): config.h

MANDOC_HTML_OBJS = eqn_html.o \
		   html.o \
		   man_html.o \
		   mdoc_html.o \
		   tbl_html.o
MANDOC_HTML_LNS	 = eqn_html.ln \
		   html.ln \
		   man_html.ln \
		   mdoc_html.ln \
		   tbl_html.ln

MANDOC_MAN_OBJS  = mdoc_man.o
MANDOC_MAN_LNS   = mdoc_man.ln

MANDOC_TERM_OBJS = eqn_term.o \
		   man_term.o \
		   mdoc_term.o \
		   term.o \
		   term_ascii.o \
		   term_ps.o \
		   tbl_term.o
MANDOC_TERM_LNS	 = eqn_term.ln \
		   man_term.ln \
		   mdoc_term.ln \
		   term.ln \
		   term_ascii.ln \
		   term_ps.ln \
		   tbl_term.ln

MANDOC_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   main.o \
		   out.o \
		   tree.o
MANDOC_LNS	 = $(MANDOC_HTML_LNS) \
		   $(MANDOC_MAN_LNS) \
		   $(MANDOC_TERM_LNS) \
		   main.ln \
		   out.ln \
		   tree.ln

$(MANDOC_HTML_OBJS) $(MANDOC_HTML_LNS): html.h
$(MANDOC_TERM_OBJS) $(MANDOC_TERM_LNS): term.h
$(MANDOC_OBJS) $(MANDOC_LNS): main.h mandoc.h mdoc.h man.h config.h out.h

MANDOCDB_OBJS	 = mandocdb.o manpath.o
MANDOCDB_LNS	 = mandocdb.ln manpath.ln

$(MANDOCDB_OBJS) $(MANDOCDB_LNS): mandocdb.h mandoc.h mdoc.h man.h config.h manpath.h

PRECONV_OBJS	 = preconv.o
PRECONV_LNS	 = preconv.ln

$(PRECONV_OBJS) $(PRECONV_LNS): config.h

APROPOS_OBJS	 = apropos.o apropos_db.o manpath.o
APROPOS_LNS	 = apropos.ln apropos_db.ln manpath.ln

$(APROPOS_OBJS) $(APROPOS_LNS): config.h mandoc.h apropos_db.h manpath.h mandocdb.h

CGI_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   cgi.o \
		   apropos_db.o \
		   manpath.o \
		   out.o \
		   tree.o

CGI_LNS	 	 = $(MANDOC_HTML_LNS) \
		   $(MANDOC_MAN_LNS) \
		   $(MANDOC_TERM_LNS) \
		   cgi.ln \
		   apropos_db.ln \
		   manpath.ln \
		   out.ln \
		   tree.ln

$(CGI_OBJS) $(CGI_LNS): main.h mdoc.h man.h out.h config.h mandoc.h apropos_db.h manpath.h mandocdb.h

CATMAN_OBJS	 = catman.o manpath.o
CATMAN_LNS 	 = catman.ln manpath.ln

$(CATMAN_OBJS) $(CATMAN_LNS): config.h mandoc.h manpath.h mandocdb.h

DEMANDOC_OBJS	 = demandoc.o
DEMANDOC_LNS	 = demandoc.ln

$(DEMANDOC_OBJS) $(DEMANDOC_LNS): config.h

INDEX_MANS	 = apropos.1.html \
		   apropos.1.xhtml \
		   apropos.1.ps \
		   apropos.1.pdf \
		   apropos.1.txt \
		   catman.8.html \
		   catman.8.xhtml \
		   catman.8.ps \
		   catman.8.pdf \
		   catman.8.txt \
		   demandoc.1.html \
		   demandoc.1.xhtml \
		   demandoc.1.ps \
		   demandoc.1.pdf \
		   demandoc.1.txt \
		   mandoc.1.html \
		   mandoc.1.xhtml \
		   mandoc.1.ps \
		   mandoc.1.pdf \
		   mandoc.1.txt \
		   whatis.1.html \
		   whatis.1.xhtml \
		   whatis.1.ps \
		   whatis.1.pdf \
		   whatis.1.txt \
		   mandoc.3.html \
		   mandoc.3.xhtml \
		   mandoc.3.ps \
		   mandoc.3.pdf \
		   mandoc.3.txt \
		   eqn.7.html \
		   eqn.7.xhtml \
		   eqn.7.ps \
		   eqn.7.pdf \
		   eqn.7.txt \
		   man.7.html \
		   man.7.xhtml \
		   man.7.ps \
		   man.7.pdf \
		   man.7.txt \
		   man.cgi.7.html \
		   man.cgi.7.xhtml \
		   man.cgi.7.ps \
		   man.cgi.7.pdf \
		   man.cgi.7.txt \
		   mandoc_char.7.html \
		   mandoc_char.7.xhtml \
		   mandoc_char.7.ps \
		   mandoc_char.7.pdf \
		   mandoc_char.7.txt \
		   mdoc.7.html \
		   mdoc.7.xhtml \
		   mdoc.7.ps \
		   mdoc.7.pdf \
		   mdoc.7.txt \
		   preconv.1.html \
		   preconv.1.xhtml \
		   preconv.1.ps \
		   preconv.1.pdf \
		   preconv.1.txt \
		   roff.7.html \
		   roff.7.xhtml \
		   roff.7.ps \
		   roff.7.pdf \
		   roff.7.txt \
		   tbl.7.html \
		   tbl.7.xhtml \
		   tbl.7.ps \
		   tbl.7.pdf \
		   tbl.7.txt \
		   mandocdb.8.html \
		   mandocdb.8.xhtml \
		   mandocdb.8.ps \
		   mandocdb.8.pdf \
		   mandocdb.8.txt

$(INDEX_MANS): mandoc

INDEX_OBJS	 = $(INDEX_MANS) \
		   man.h.html \
		   mandoc.h.html \
		   mdoc.h.html \
		   mdocml.tar.gz \
		   mdocml.md5

www: index.html

lint: llib-lmandoc.ln llib-lpreconv.ln llib-ldemandoc.ln $(DBLN)

clean:
	rm -f libmandoc.a $(LIBMANDOC_OBJS)
	rm -f llib-llibmandoc.ln $(LIBMANDOC_LNS)
	rm -f mandocdb $(MANDOCDB_OBJS)
	rm -f llib-lmandocdb.ln $(MANDOCDB_LNS)
	rm -f preconv $(PRECONV_OBJS)
	rm -f llib-lpreconv.ln $(PRECONV_LNS)
	rm -f apropos whatis $(APROPOS_OBJS)
	rm -f llib-lapropos.ln $(APROPOS_LNS)
	rm -f man.cgi $(CGI_OBJS)
	rm -f llib-lman.cgi.ln $(CGI_LNS)
	rm -f catman $(CATMAN_OBJS)
	rm -f llib-lcatman.ln $(CATMAN_LNS)
	rm -f demandoc $(DEMANDOC_OBJS)
	rm -f llib-ldemandoc.ln $(DEMANDOC_LNS)
	rm -f mandoc $(MANDOC_OBJS)
	rm -f llib-lmandoc.ln $(MANDOC_LNS)
	rm -f config.h config.log $(COMPAT_OBJS) $(COMPAT_LNS)
	rm -f mdocml.tar.gz mdocml-win32.zip mdocml-win64.zip mdocml-macosx.zip
	rm -f index.html $(INDEX_OBJS)
	rm -rf test-fgetln.dSYM
	rm -rf test-strlcpy.dSYM
	rm -rf test-strlcat.dSYM 
	rm -rf test-strptime.dSYM 
	rm -rf test-mmap.dSYM 
	rm -rf test-getsubopt.dSYM
	rm -rf apropos.dSYM
	rm -rf catman.dSYM
	rm -rf mandocdb.dSYM
	rm -rf whatis.dSYM

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
	$(INSTALL_MAN) mandoc.3 $(DESTDIR)$(MANDIR)/man3
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

llib-llibmandoc.ln: $(COMPAT_LNS) $(LIBMANDOC_LNS)
	$(LINT) $(LINTFLAGS) -Clibmandoc $(COMPAT_LNS) $(LIBMANDOC_LNS)

mandoc: $(MANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOC_OBJS) libmandoc.a

llib-lmandoc.ln: $(MANDOC_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Cmandoc $(MANDOC_LNS) llib-llibmandoc.ln

mandocdb: $(MANDOCDB_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOCDB_OBJS) libmandoc.a $(DBLIB)

llib-lmandocdb.ln: $(MANDOCDB_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Cmandocdb $(MANDOCDB_LNS) llib-llibmandoc.ln

preconv: $(PRECONV_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(PRECONV_OBJS)

llib-lpreconv.ln: $(PRECONV_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Cpreconv $(PRECONV_LNS) llib-llibmandoc.ln

whatis: apropos
	cp -f apropos whatis

apropos: $(APROPOS_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(APROPOS_OBJS) libmandoc.a $(DBLIB)

llib-lapropos.ln: $(APROPOS_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Capropos $(APROPOS_LNS) llib-llibmandoc.ln

catman: $(CATMAN_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(CATMAN_OBJS) libmandoc.a $(DBLIB)

llib-lcatman.ln: $(CATMAN_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Ccatman $(CATMAN_LNS) llib-llibmandoc.ln

man.cgi: $(CGI_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) $(STATIC) -o $@ $(CGI_OBJS) libmandoc.a $(DBLIB)

llib-lman.cgi.ln: $(CGI_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Cman.cgi $(CGI_LNS) llib-llibmandoc.ln

demandoc: $(DEMANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(DEMANDOC_OBJS) libmandoc.a

llib-ldemandoc.ln: $(DEMANDOC_LNS) llib-llibmandoc.ln
	$(LINT) $(LINTFLAGS) -Cdemandoc $(DEMANDOC_LNS) llib-llibmandoc.ln

mdocml.md5: mdocml.tar.gz
	md5 mdocml.tar.gz >$@

mdocml.tar.gz: $(SRCS)
	mkdir -p .dist/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .dist/mdocml-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

mdocml-win32.zip: $(SRCS)
	mkdir -p .win32/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .win32
	cp .win32/Makefile .win32/Makefile.old
	egrep -v -e DUSE_WCHAR -e ^DBBIN .win32/Makefile.old >.win32/Makefile
	( cd .win32; \
		CC=i686-w64-mingw32-gcc AR=i686-w64-mingw32-ar CFLAGS='-DOSNAME=\"Windows\"' make; \
		make install PREFIX=mdocml-$(VERSION) ; \
		zip -r ../$@ mdocml-$(VERSION) )
	rm -rf .win32

mdocml-win64.zip: $(SRCS)
	mkdir -p .win64/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .win64
	cp .win64/Makefile .win64/Makefile.old
	egrep -v -e DUSE_WCHAR -e ^DBBIN .win64/Makefile.old >.win64/Makefile
	( cd .win64; \
		CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar CFLAGS='-DOSNAME=\"Windows\"' make; \
		make install PREFIX=mdocml-$(VERSION) ; \
		zip -r ../$@ mdocml-$(VERSION) )
	rm -rf .win64

mdocml-macosx.zip: $(SRCS)
	mkdir -p .macosx/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .macosx
	( cd .macosx; \
		CFLAGS="-arch i386 -arch x86_64 -arch ppc" LDFLAGS="-arch i386 -arch x86_64 -arch ppc" make; \
		make install PREFIX=mdocml-$(VERSION) ; \
		zip -r ../$@ mdocml-$(VERSION) )
	rm -rf .macosx

index.html: $(INDEX_OBJS)

config.h: config.h.pre config.h.post
	rm -f config.log
	( cat config.h.pre; \
	  echo; \
	  if $(CC) $(CFLAGS) -Werror -o test-fgetln test-fgetln.c >> config.log 2>&1; then \
		echo '#define HAVE_FGETLN'; \
		rm test-fgetln; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -o test-strptime test-strptime.c >> config.log 2>&1; then \
		echo '#define HAVE_STRPTIME'; \
		rm test-strptime; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -o test-getsubopt test-getsubopt.c >> config.log 2>&1; then \
		echo '#define HAVE_GETSUBOPT'; \
		rm test-getsubopt; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -o test-strlcat test-strlcat.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCAT'; \
		rm test-strlcat; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -o test-mmap test-mmap.c >> config.log 2>&1; then \
		echo '#define HAVE_MMAP'; \
		rm test-mmap; \
	  fi; \
	  if $(CC) $(CFLAGS) -Werror -o test-strlcpy test-strlcpy.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCPY'; \
		rm test-strlcpy; \
	  fi; \
	  echo; \
	  cat config.h.post \
	) > $@

.h.h.html:
	highlight -I $< >$@

.1.1.txt .3.3.txt .7.7.txt .8.8.txt:
	./mandoc -Tascii -Wall,stop $< | col -b >$@

.1.1.html .3.3.html .7.7.html .8.8.html:
	./mandoc -Thtml -Wall,stop -Ostyle=style.css,man=%N.%S.html,includes=%I.html $< >$@

.1.1.ps .3.3.ps .7.7.ps .8.8.ps:
	./mandoc -Tps -Wall,stop $< >$@

.1.1.xhtml .3.3.xhtml .7.7.xhtml .8.8.xhtml:
	./mandoc -Txhtml -Wall,stop -Ostyle=style.css,man=%N.%S.xhtml,includes=%I.html $< >$@

.1.1.pdf .3.3.pdf .7.7.pdf .8.8.pdf:
	./mandoc -Tpdf -Wall,stop $< >$@

.sgml.html:
	validate --warn $<
	sed -e "s!@VERSION@!$(VERSION)!" -e "s!@VDATE@!$(VDATE)!" $< >$@
