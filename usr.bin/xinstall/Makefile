#	@(#)Makefile	5.5 (Berkeley) 6/24/90

PROG=	xinstall
MAN1=	install.1
CLEANFILES=xinstall

install: maninstall
	install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>


