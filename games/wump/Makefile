#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/wump/Makefile,v 1.3.6.1 2001/04/25 09:29:04 ru Exp $

PROG=	wump
MAN=	wump.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		${.CURDIR}/wump.info ${DESTDIR}${SHAREDIR}/games

.include <bsd.prog.mk>
