#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	wump
MAN6=	wump.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/wump.info \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
