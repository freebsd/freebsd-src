#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	canfield
MAN=	canfield.6
DPADD=	${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lcurses -lcompat
HIDEGAME=hidegame
MLINKS=	canfield.6 cfscores.6

beforeinstall:
.if !exists(${DESTDIR}/var/games/cfscores)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/cfscores
.endif

.include "../../Makefile.inc"
.include <bsd.prog.mk>
