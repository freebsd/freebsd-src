#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/rogue/Makefile,v 1.4.2.1 2000/07/19 19:31:41 billf Exp $

PROG=	rogue
CFLAGS+=-DUNIX -fwritable-strings
SRCS=	curses.c hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermcap
HIDEGAME=hidegame
MAN6=	rogue.6

beforeinstall:
.if !exists(${DESTDIR}/var/games/rogue.scores)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/rogue.scores
.endif

.include <bsd.prog.mk>
