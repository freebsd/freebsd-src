#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/robots/Makefile,v 1.5 1999/12/10 21:13:39 billf Exp $

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c flush_in.c
MAN6=	robots.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame

beforeinstall:
.if !exists(${DESTDIR}/var/games/robots_roll)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/robots_roll
.endif

.include <bsd.prog.mk>
