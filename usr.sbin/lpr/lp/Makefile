#	$Id: Makefile,v 1.4 1997/12/02 20:45:33 wollman Exp $

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
