#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fish/Makefile,v 1.4.6.1 2001/04/25 09:28:54 ru Exp $

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	  ${.CURDIR}/fish.instr  ${DESTDIR}${SHAREDIR}/games/fish.instr

.include <bsd.prog.mk>
