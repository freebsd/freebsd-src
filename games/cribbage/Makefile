#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/cribbage/Makefile,v 1.7 1999/12/10 21:13:35 billf Exp $

PROG=	cribbage
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
MAN6=	cribbage.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	  ${.CURDIR}/cribbage.n  ${DESTDIR}${SHAREDIR}/games/cribbage.instr
.if !exists(${DESTDIR}/var/games/criblog)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/criblog
.endif

.include <bsd.prog.mk>
