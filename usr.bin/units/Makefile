#	$Id: Makefile,v 1.3 1997/02/22 19:57:30 peter Exp $

PROG=	units

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
