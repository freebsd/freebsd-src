#	@(#)Makefile	5.5 (Berkeley) 7/1/90

MAN1=	which.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.csh ${DESTDIR}${BINDIR}/which

.include <bsd.prog.mk>
