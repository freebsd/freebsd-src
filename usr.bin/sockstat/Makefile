#	$Id: Makefile,v 1.1 1999/04/14 16:17:34 des Exp $

MAINTAINER=	des@freebsd.org
MAN1=		sockstat.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sockstat.pl ${DESTDIR}${BINDIR}/sockstat

.include <bsd.prog.mk>
