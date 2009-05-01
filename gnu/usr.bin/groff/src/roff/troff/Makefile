# $FreeBSD: src/gnu/usr.bin/groff/src/roff/troff/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	troff
SRCS=		dictionary.cpp div.cpp env.cpp input.cpp \
		majorminor.cpp mtsm.cpp node.cpp number.cpp reg.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	majorminor.cpp ${MAN}

.include <bsd.prog.mk>

majorminor.cpp: ${GROFF_DIST}/VERSION ${GROFF_DIST}/REVISION
	@${ECHO} Making $@
	@-rm -f $@
	@echo const char \*major_version = \
	\"`sed -e 's/^\([^.]*\)\..*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*minor_version = \
	\"`sed -e 's/^[^.]*\.\([0-9]*\).*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >>$@
	@echo const char \*revision = \"`cat ${GROFF_DIST}/REVISION`\"\; >>$@
