#	$Id: Makefile,v 1.4 1998/09/20 21:38:12 bde Exp $

PROG=	units

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
