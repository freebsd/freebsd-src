# $FreeBSD$

LIB=		groff
INTERNALLIB=
SRCS=		assert.cpp change_lf.cpp cmap.cpp color.cpp cset.cpp\
		device.cpp errarg.cpp error.cpp fatal.cpp filename.cpp\
		font.cpp fontfile.cpp geometry.cpp glyphuni.cpp htmlhint.cpp\
		hypot.cpp invalid.cpp lf.cpp lineno.cpp macropath.cpp\
		maxfilename.cpp mksdir.cpp nametoindex.cpp new.cpp paper.cpp prime.cpp\
	 	ptable.cpp searchpath.cpp string.cpp strsave.cpp symbol.cpp\
		tmpfile.cpp tmpname.cpp unicode.cpp uniglyph.cpp uniuni.cpp version.cpp
SRCS+=		iftoa.c itoa.c matherr.c progname.c
CLEANFILES=	version.cpp

.include <bsd.lib.mk>

version.cpp: ${GROFF_DIST}/VERSION ${GROFF_DIST}/REVISION
	@${ECHO} Making version.cpp
	@echo const char \*version_string = \"`cat ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*revision_string = \"`cat ${GROFF_DIST}/REVISION`\"\; >>$@
	@echo extern \"C\" const char \*Version_string = \"`cat ${GROFF_DIST}/VERSION`.`cat ${GROFF_DIST}/REVISION`\"\; | \
	  sed -e 's/\.0\"/\"/' >>$@
