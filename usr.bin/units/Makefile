#	$Id: Makefile,v 1.2 1996/04/06 06:00:58 thorpej Exp $

PROG=	units

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
