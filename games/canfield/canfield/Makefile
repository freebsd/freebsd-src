#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/canfield/canfield/Makefile,v 1.5 1999/12/10 21:13:35 billf Exp $

PROG=	canfield
MAN6=	canfield.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame
MLINKS=	canfield.6 cfscores.6

beforeinstall:
.if !exists(${DESTDIR}/var/games/cfscores)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/cfscores
.endif

.include "../../Makefile.inc"
.include <bsd.prog.mk>
