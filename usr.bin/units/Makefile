# $FreeBSD: src/usr.bin/units/Makefile,v 1.4.2.1 1999/08/29 15:34:15 peter Exp $

PROG=	units

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
