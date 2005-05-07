# $FreeBSD: src/gnu/usr.bin/groff/src/libs/libgroff/Makefile,v 1.7.2.1 2005/02/13 07:23:09 obrien Exp $

LIB=		groff
INTERNALLIB=
SRCS=		assert.cpp change_lf.cpp cmap.cpp color.cpp cset.cpp device.cpp errarg.cpp\
		error.cpp fatal.cpp filename.cpp font.cpp fontfile.cpp geometry.cpp\
		htmlhint.cpp invalid.cpp lf.cpp lineno.cpp macropath.cpp\
		maxfilename.cpp mksdir.cpp nametoindex.cpp new.cpp paper.cpp prime.cpp\
	 	progname.cpp ptable.cpp searchpath.cpp string.cpp strsave.cpp\
		tmpfile.cpp tmpname.cpp version.cpp
SRCS+=		iftoa.c itoa.c matherr.c
CLEANFILES=	version.cpp

.include <bsd.lib.mk>

version.cpp: ${GROFF_DIST}/VERSION ${GROFF_DIST}/REVISION
	@${ECHO} Making version.cpp
	@echo const char \*version_string = \"`cat ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*revision_string = \"`cat ${GROFF_DIST}/REVISION`\"\; >>$@
	@echo extern \"C\" const char \*Version_string = \"`cat ${GROFF_DIST}/VERSION`.`cat ${GROFF_DIST}/REVISION`\"\; | \
	  sed -e 's/\.0\"/\"/' >>$@
