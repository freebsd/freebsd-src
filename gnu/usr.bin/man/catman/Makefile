# $FreeBSD: src/gnu/usr.bin/man/catman/Makefile,v 1.11 1999/08/27 23:36:08 peter Exp $

MAINTAINER= wosch


MAN1=	catman.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/catman.perl ${DESTDIR}${BINDIR}/catman

.include <bsd.prog.mk>
