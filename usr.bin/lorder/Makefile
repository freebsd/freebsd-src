#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAN=	lorder.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/lorder.sh ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
