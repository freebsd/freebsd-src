#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/wargames/Makefile,v 1.7.2.1 2001/03/05 12:12:24 kris Exp $

MAN6=	wargames.6

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}${BINDIR}/wargames

.include <bsd.prog.mk>
