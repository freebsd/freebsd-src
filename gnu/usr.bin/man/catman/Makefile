# $FreeBSD$

MAINTAINER= wosch


MAN=	catman.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/catman.perl ${DESTDIR}${BINDIR}/catman

.include <bsd.prog.mk>
