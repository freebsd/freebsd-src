# $FreeBSD$

LIB=		groff
SRCS=		assert.cc change_lf.cc cmap.cc cset.cc device.cc errarg.cc\
		error.cc fatal.cc filename.cc font.cc fontfile.cc\
		htmlindicate.cc lf.cc\
		lineno.cc macropath.cc nametoindex.cc new.cc prime.cc\
	 	progname.cc ptable.cc searchpath.cc string.cc strsave.cc\
		tmpfile.cc illegal.cc version.cc
SRCS+=		getopt.c getopt1.c iftoa.c itoa.c matherr.c
CFLAGS+=	-I$(.CURDIR)/../include

NOMAN=		noman
NOPROFILE=	noprofile

CLEANFILES+=	version.cc

install:

.include "../Makefile.cfg"
.include <bsd.lib.mk>

version.cc: $(GROFF_DIST)/VERSION $(GROFF_DIST)/REVISION
	@${ECHO} Making version.cc
	@echo const char \*version_string = \"`cat $(GROFF_DIST)/VERSION`\"\; >$@
	@echo const char \*revision_string = \"`cat $(GROFF_DIST)/REVISION`\"\; >>$@
	@echo const char \*Version_string = \"`cat $(GROFF_DIST)/VERSION`.`cat $(GROFF_DIST)/REVISION`\"\; | \
	  sed -e 's/\.0\"/\"/' >>$@
