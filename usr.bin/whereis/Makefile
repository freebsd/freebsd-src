# $FreeBSD$

MAN=	whereis.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whereis.pl ${DESTDIR}${BINDIR}/whereis

.include <bsd.prog.mk>
