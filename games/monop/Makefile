#	@(#)Makefile	5.5 (Berkeley) 5/11/90

PROG=	monop
SRCS=	monop.c cards.c execute.c getinp.c houses.c jail.c misc.c morg.c \
	print.c prop.c rent.c roll.c spec.c trade.c
MAN6=	monop.6
HIDEGAME=hidegame
CLEANFILES+=initdeck cards.pck

all: cards.pck

cards.pck: initdeck
	./initdeck ${.CURDIR}/cards.inp

initdeck: initdeck.c
	${CC} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/initdeck.c

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 cards.pck \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
