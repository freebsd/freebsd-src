#	$Id$

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
