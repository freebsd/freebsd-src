# $FreeBSD$

MAN=		pkg_update.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_update.pl ${DESTDIR}${BINDIR}/pkg_update

.include <bsd.prog.mk>
