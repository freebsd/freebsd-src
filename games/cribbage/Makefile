#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	cribbage
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
MAN6=	cribbage.6
HIDEGAME=hidegame

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/cribbage.n \
	    ${DESTDIR}/usr/share/games/cribbage.instr

.include <bsd.prog.mk>
