#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/which/Makefile,v 1.5.2.1 2001/04/25 11:29:46 ru Exp $

MAINTAINER= wosch

MAN=	which.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.pl ${DESTDIR}${BINDIR}/which

.include <bsd.prog.mk>
