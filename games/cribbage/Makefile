#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	cribbage
DPADD=	${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lcurses -lcompat
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
MAN=	cribbage.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
	  ${.CURDIR}/cribbage.n  ${DESTDIR}${SHAREDIR}/games/cribbage.instr
.if !exists(${DESTDIR}/var/games/criblog)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/criblog
.endif

.include <bsd.prog.mk>
