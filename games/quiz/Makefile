#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/quiz/Makefile,v 1.5.6.1 2001/04/25 09:29:00 ru Exp $

PROG=	quiz
SRCS=	quiz.c rxp.c
MAN=	quiz.6
CATS=	africa america areas arith asia babies bard chinese collectives \
	ed elements europe flowers greek inca index latin locomotive \
	midearth morse murders poetry posneg pres province seq-easy seq-hard \
	sexes sov spell state trek ucc
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m ${NOBINMODE} ${CATS} ${DESTDIR}${SHAREDIR}/games/quiz.db)

.include <bsd.prog.mk>
