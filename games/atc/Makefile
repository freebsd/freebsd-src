#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/atc/Makefile,v 1.14 2000/02/25 10:23:19 mph Exp $

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.y graphics.c input.c lex.l list.c log.c \
	main.c tunable.c update.c y.tab.h
MAN6=	atc.6
DPADD=	${LIBL} ${LIBM} ${LIBCURSES} ${LIBTERMCAP}
LDADD=	-ll -lm -lcurses -ltermcap
GAMES=  Game_List Killer crossover default easy game_2
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/games; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${GAMES} ${DESTDIR}${SHAREDIR}/games/atc)

.include <bsd.prog.mk>
