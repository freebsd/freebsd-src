#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.y graphics.c input.c lex.l list.c log.c \
	main.c tunable.c update.c y.tab.h
MAN=	atc.6
DPADD=	${LIBL} ${LIBM} ${LIBCURSES} ${LIBTERMCAP}
LDADD=	-ll -lm -lcurses -ltermcap
GAMES=  Game_List Killer crossover default easy game_2
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/games; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${GAMES} ${DESTDIR}${SHAREDIR}/games/atc)

.include <bsd.prog.mk>
