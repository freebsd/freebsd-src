#	$Id: Makefile,v 1.1 1999/04/15 13:40:43 des Exp $

MAINTAINER=	des@freebsd.org
MAN1=		sockstat.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sockstat.pl ${DESTDIR}${BINDIR}/sockstat

.include <bsd.prog.mk>
