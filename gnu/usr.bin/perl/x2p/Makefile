
PROG= a2p

SRCS+= a2p.c hash.c str.c walk.c util.c
CFLAGS+= -I${.CURDIR}/../perl
CLEANFILES+= y.tab.h a2p.c

LDADD= -lm
DPADD= ${LIBM}

MAN1+= a2p.1 s2p.1 h2ph.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/s2p \
		${DESTDIR}${BINDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/h2ph \
		${DESTDIR}${BINDIR}

afterinstall:
	(DESTDIR=${DESTDIR}; cd ${DESTDIR}/usr/include; ${BINDIR}/h2ph * sys/*)

.include "../../Makefile.inc"
.include <bsd.prog.mk>
