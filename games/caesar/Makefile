#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	caesar
MAN6=	caesar.6
DPADD=	${LIBM}
LDADD=	-lm
MLINKS=	caesar.6 rot13.6

CFLAGS+= -Wall

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/rot13.sh ${DESTDIR}/usr/games/rot13

.include <bsd.prog.mk>
