#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD: src/usr.bin/keyinfo/Makefile,v 1.4.6.1 2001/04/25 11:29:24 ru Exp $

BINMODE=4555

MAN=	keyinfo.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/keyinfo.pl ${DESTDIR}${BINDIR}/keyinfo

.include <bsd.prog.mk>
