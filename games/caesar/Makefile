#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	caesar
DPADD=	${LIBM}
LDADD=	-lm
MAN=	caesar.6
MLINKS=	caesar.6 rot13.6

CFLAGS+= -Wall

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/rot13.sh ${DESTDIR}${BINDIR}/rot13

.include <bsd.prog.mk>
