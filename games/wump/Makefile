#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	wump
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		${.CURDIR}/wump.info ${DESTDIR}${SHAREDIR}/games

.include <bsd.prog.mk>
