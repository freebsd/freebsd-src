#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN1=	which.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.pl ${DESTDIR}/${BINDIR}/which

.include <bsd.prog.mk>
