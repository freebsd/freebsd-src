# $FreeBSD$

PROG=		troff
SRCS=		env.cc node.cc input.cc div.cc symbol.cc dictionary.cc reg.cc \
		number.cc majorminor.cc
DPADD=		${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBGROFF} -lm -lstdc++
CLEANFILES=	majorminor.cc ${MAN}

.include <bsd.prog.mk>

majorminor.cc: ${GROFF_DIST}/VERSION ${GROFF_DIST}/REVISION
	@${ECHO} Making $@
	@-rm -f $@
	@echo const char \*major_version = \
	\"`sed -e 's/^\([^.]*\)\..*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*minor_version = \
	\"`sed -e 's/^[^.]*\.\([0-9]*\).*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >>$@
	@echo const char \*revision = \"`cat ${GROFF_DIST}/REVISION`\"\; >>$@
