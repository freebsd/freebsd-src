#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.y graphics.c input.c lex.l list.c log.c \
	main.c tunable.c update.c
MAN6=	atc.6
DPADD=	${LIBL} ${LIBM} ${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-ll -lm -lcurses -ltermcap -lcompat
GAMES=  Game_List Killer crossover default easy game_2
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/games; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 440 \
	    ${GAMES} ${DESTDIR}${SHAREDIR}/games/atc)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${.CURDIR}/games/ATC_scores \
	    ${DESTDIR}/var/games/atc_score

.include <bsd.prog.mk>
