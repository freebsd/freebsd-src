#	$Id$

PROG= a2p

SRCS= a2p.y hash.c str.c walk.c util.c
CFLAGS+= -I${.CURDIR}/../perl

LDADD= -lm
DPADD= ${LIBM}

MAN1+= a2p.1 s2p.1 h2ph.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/s2p \
		${DESTDIR}${BINDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/h2ph \
		${DESTDIR}${BINDIR}

afterinstall:
	(DESTDIR=${DESTDIR}; cd ${DESTDIR}/usr/include; ${BINDIR}/h2ph * machine/* sys/*)

.include "../../Makefile.inc"
.include <bsd.prog.mk>
