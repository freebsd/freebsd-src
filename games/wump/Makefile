#	@(#)Makefile	5.4 (Berkeley) 5/11/90

PROG=	wump
MAN6=	wump.6
HIDEGAME=hidegame

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/wump.info \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
