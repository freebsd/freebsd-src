#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/id/Makefile,v 1.5.6.2 2001/07/30 09:59:15 dd Exp $

PROG=	id
WARNS?=	2
MAN=	id.1 groups.1 whoami.1

# XXX BROKEN:	afterinstall:
afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/groups.sh ${DESTDIR}${BINDIR}/groups
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whoami.sh ${DESTDIR}${BINDIR}/whoami

.include <bsd.prog.mk>
