#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

MAN6=	wargames.6

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}${BINDIR}/wargames

.include <bsd.prog.mk>
