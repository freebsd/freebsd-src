#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	monop
SRCS=	monop.c cards.c execute.c getinp.c houses.c jail.c misc.c morg.c \
	print.c prop.c rent.c roll.c spec.c trade.c
MAN6=	monop.6
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat
HIDEGAME=hidegame
CLEANFILES+=initdeck cards.pck

all: ${PROG} ${MAN6}

monop: cards.pck

cards.pck: initdeck
	./initdeck ${.CURDIR}/cards.inp

initdeck: initdeck.c
	${CC} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/initdeck.c

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 cards.pck \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
