#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.y graphics.c input.c lex.l list.c log.c \
	main.c tunable.c update.c y.tab.h
MAN=	atc.6
DPADD=	${LIBL} ${LIBM} ${LIBCURSES}
LDADD=	-ll -lm -lcurses
FILES=  Game_List Killer crossover default easy game_2
FILESDIR=	${SHAREDIR}/games/atc
HIDEGAME=hidegame

.PATH: ${.CURDIR}/games

.include <bsd.prog.mk>
