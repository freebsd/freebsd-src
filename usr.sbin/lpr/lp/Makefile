# $FreeBSD: src/usr.sbin/lpr/lp/Makefile,v 1.4.2.1 1999/08/29 15:43:16 peter Exp $

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
