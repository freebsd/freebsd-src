#	@(#)Makefile	5.10 (Berkeley) 5/11/90

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c flush_in.c
MAN6=	robots.6
DPADD=	${LIBCURSES} ${LIBTERM}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/games/robots_roll

.include <bsd.prog.mk>
