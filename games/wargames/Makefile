#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/wargames/Makefile,v 1.7 2000/02/15 07:15:41 jkh Exp $

MAN6=	wargames.6

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${.CURDIR}/wargames.sh \
	    ${DESTDIR}${BINDIR}/wargames

.include <bsd.prog.mk>
