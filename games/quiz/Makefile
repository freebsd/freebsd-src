#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/quiz/Makefile,v 1.5.6.3 2002/02/18 16:02:43 ru Exp $

PROG=	quiz
SRCS=	quiz.c rxp.c
MAN=	quiz.6
FILES=	africa america areas arith asia babies bard chinese collectives \
	ed elements europe flowers greek inca index latin locomotive \
	midearth morse murders poetry posneg pres province seq-easy seq-hard \
	sexes sov spell state trek ucc
FILESDIR=	${SHAREDIR}/games/quiz.db
HIDEGAME=hidegame

.PATH: ${.CURDIR}/datfiles

.include <bsd.prog.mk>
