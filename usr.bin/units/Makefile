#	$Id: Makefile,v 1.1.1.1 1996/06/08 03:43:43 alex Exp $

PROG=	units

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
