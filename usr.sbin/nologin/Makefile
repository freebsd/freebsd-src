#	@(#)Makefile	8.2 (Berkeley) 4/22/94
# $FreeBSD$

MAN=	nologin.5 nologin.8

nologin depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}${BINDIR}/nologin

.include <bsd.prog.mk>
