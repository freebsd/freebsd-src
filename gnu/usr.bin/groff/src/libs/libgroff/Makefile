# $FreeBSD$

LIB=		groff
INTERNALLIB=	YES
SRCS=		assert.cc change_lf.cc cmap.cc color.cc cset.cc device.cc errarg.cc\
		error.cc fatal.cc filename.cc font.cc fontfile.cc geometry.cc\
		htmlhint.cc invalid.cc lf.cc lineno.cc macropath.cc\
		maxfilename.cc mksdir.cc nametoindex.cc new.cc paper.cc prime.cc\
	 	progname.cc ptable.cc searchpath.cc string.cc strsave.cc\
		tmpfile.cc tmpname.cc version.cc
SRCS+=		getopt.c getopt1.c iftoa.c itoa.c matherr.c
CLEANFILES=	version.cc

.include <bsd.lib.mk>

version.cc: ${GROFF_DIST}/VERSION ${GROFF_DIST}/REVISION
	@${ECHO} Making version.cc
	@echo const char \*version_string = \"`cat ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*revision_string = \"`cat ${GROFF_DIST}/REVISION`\"\; >>$@
	@echo extern \"C\" const char \*Version_string = \"`cat ${GROFF_DIST}/VERSION`.`cat ${GROFF_DIST}/REVISION`\"\; | \
	  sed -e 's/\.0\"/\"/' >>$@
