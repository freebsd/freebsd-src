#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/wargames/Makefile,v 1.3.2.3 2000/02/16 17:29:20 jkh Exp $

MAN6=	wargames.6

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}${BINDIR}/wargames

.include <bsd.prog.mk>
