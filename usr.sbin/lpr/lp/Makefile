#       @(#)Makefile    5.6 (Berkeley) 6/23/90

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
