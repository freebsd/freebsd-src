#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	  ${.CURDIR}/fish.instr  ${DESTDIR}${SHAREDIR}/games/fish.instr

.include <bsd.prog.mk>
