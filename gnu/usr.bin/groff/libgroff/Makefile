# Makefile for libgroff
#
# $FreeBSD$
#
.include "../Makefile.inc"

LIB=		groff
SRCS=		assert.cc change_lf.cc cmap.cc cset.cc device.cc errarg.cc\
		error.cc fatal.cc filename.cc font.cc fontfile.cc lf.cc\
		lineno.cc macropath.cc nametoindex.cc new.cc prime.cc\
	 	progname.cc ptable.cc searchpath.cc string.cc strsave.cc\
		tmpfile.cc illegal.cc version.cc
SRCS+=		iftoa.c itoa.c matherr.c
CFLAGS+=	-I$(.CURDIR)/../include

NOMAN=		noman
NOPROFILE=	noprofile

CLEANFILES+=	version.cc

version.cc: $(GROFF_DIST)/VERSION
	@${ECHO} Making version.cc
	@echo const char \*version_string = \"`cat $(GROFF_DIST)/VERSION`\"\; >$@

install:

.include "../Makefile.cfg"
.include <bsd.lib.mk>
