#	$Id: Makefile,v 1.14 1998/05/12 23:02:14 jb Exp $

PROG= a2p

SRCS= a2p.y hash.c str.c walk.c util.c
CFLAGS+= -I${.CURDIR}/../perl
YFLAGS=

LDADD= -lm
DPADD= ${LIBM}

MAN1+= a2p.1 s2p.1 h2ph.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/s2p \
		${DESTDIR}${BINDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/h2ph \
		${DESTDIR}${BINDIR}

afterinstall:
	(DESTDIR=${DESTDIR}; cd ${DESTDIR}/usr/include; \
	    perl ${DESTDIR}${BINDIR}/h2ph * machine/* sys/*)

.include "../../Makefile.inc"
.include <bsd.prog.mk>
