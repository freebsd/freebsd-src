# $FreeBSD$

MAINTAINER=	des@freebsd.org
MAN=		sockstat.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sockstat.pl ${DESTDIR}${BINDIR}/sockstat

.include <bsd.prog.mk>
