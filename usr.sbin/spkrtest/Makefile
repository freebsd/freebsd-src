# $FreeBSD: src/usr.sbin/spkrtest/Makefile,v 1.8 1999/08/28 01:20:01 peter Exp $

MAINTAINER= wosch

MAN8=	spkrtest.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spkrtest.pl ${DESTDIR}${BINDIR}/spkrtest

.include <bsd.prog.mk>
