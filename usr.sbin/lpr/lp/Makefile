# $FreeBSD: src/usr.sbin/lpr/lp/Makefile,v 1.5 1999/08/28 01:16:50 peter Exp $

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
