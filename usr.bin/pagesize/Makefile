#	@(#)Makefile	8.2 (Berkeley) 4/3/94
# $FreeBSD$

MAN=	pagesize.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pagesize.sh ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
