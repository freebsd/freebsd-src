#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	fish
MAN6=	fish.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/fish.instr \
	    ${DESTDIR}/usr/share/games/fish.instr

.include <bsd.prog.mk>
