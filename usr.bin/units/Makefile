# $FreeBSD: src/usr.bin/units/Makefile,v 1.5 1999/08/28 01:07:09 peter Exp $

PROG=	units

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
