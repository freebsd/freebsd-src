# $FreeBSD: src/usr.bin/sockstat/Makefile,v 1.1.2.1 1999/08/29 15:32:48 peter Exp $

MAINTAINER=	des@freebsd.org
MAN1=		sockstat.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sockstat.pl ${DESTDIR}${BINDIR}/sockstat

.include <bsd.prog.mk>
