#	$Id: Makefile,v 1.9 1997/08/23 05:26:32 jmg Exp $

PROG=		troff
SRCS=		env.cc node.cc input.cc div.cc symbol.cc dictionary.cc reg.cc \
		number.cc majorminor.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}
MANDEPEND=	troff.1

CLEANFILES+=	majorminor.cc ${MANDEPEND}

.include "../Makefile.inc"

majorminor.cc: ${GROFF_DIST}/VERSION
	@${ECHO} Making $@
	@-rm -f $@
	@echo const char \*major_version = \
	\"`sed -e 's/^\([^.]*\)\..*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >$@
	@echo const char \*minor_version = \
	\"`sed -e 's/^[^.]*\.\([0-9]*\).*$$/\1/' ${GROFF_DIST}/VERSION`\"\; >>$@

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${DIST_DIR}/hyphen.us \
		${DESTDIR}${tmacdir}/hyphen.us
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${DIST_DIR}/hyphen.us-ru ${DESTDIR}${tmacdir}/hyphen.us-ru

.include "../Makefile.cfg"
.include <bsd.prog.mk>
