#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD$

BINMODE=4555

MAN=	keyinfo.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/keyinfo.pl ${DESTDIR}${BINDIR}/keyinfo

.include <bsd.prog.mk>
