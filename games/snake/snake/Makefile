#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	snake
SRCS=	snake.c move.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lm -ltermcap -lcompat
HIDEGAME=hidegame
MLINKS=	snake.6 snscore.6

beforeinstall:
.if !exists(${DESTDIR}/var/games/snakerawscores)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/snakerawscores
.endif
.if !exists(${DESTDIR}/var/games/snake.log)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/snake.log
.endif

.include "../../Makefile.inc"
.include <bsd.prog.mk>
