#	$Id: Makefile,v 1.3.2.1 1998/07/18 11:10:40 jkh Exp $

BINDIR = /usr/bin
MAN1 = lp.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/lp.sh ${DESTDIR}${BINDIR}/lp

.include <bsd.prog.mk>
