# $FreeBSD$

MAINTAINER= wosch

MAN=	spkrtest.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spkrtest.pl ${DESTDIR}${BINDIR}/spkrtest

.include <bsd.prog.mk>
