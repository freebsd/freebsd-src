
PROG= a2p

SRCS+= a2p.c hash.c str.c walk.c util.c
CFLAGS+= -I${.CURDIR}/../perl

LDADD= -lm
DPADD= ${LIBM}

MAN1+= a2p.1 s2p.1 h2ph.1  

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 s2p ${BINDIR}
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 h2ph ${BINDIR}

.include <bsd.prog.mk>

