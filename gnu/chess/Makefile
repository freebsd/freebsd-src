#	@(#)Makefile	5.4 (Berkeley) 5/11/90

PROG=	chess
SRCS=	gnuchess.c uxdsp.c move.c
CFLAGS+=-DNEWMOVE=12
MAN6=	chess.6
DPADD=	${LIBCURSES} ${LIBTERM}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame
BINOWN?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	4700
.else
BINDIR?=	/usr/games
.endif

beforeinstall: 
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/gnuchess.book \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
