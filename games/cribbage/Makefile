#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	cribbage
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
FILES=	cribbage.n
FILESNAME_cribbage.n=	cribbage.instr
MAN=	cribbage.6
HIDEGAME=hidegame

beforeinstall:
.if !exists(${DESTDIR}/var/games/criblog)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/criblog
.endif

.include <bsd.prog.mk>
