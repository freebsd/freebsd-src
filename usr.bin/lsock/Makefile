#	$Id$

MAINTAINER=	des@freebsd.org
MAN1=		lsock.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/lsock.pl ${DESTDIR}${BINDIR}/lsock

.include <bsd.prog.mk>
