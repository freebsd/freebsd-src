#	@(#)Makefile	5.2 (Berkeley) 5/11/90

PROG=	locate

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/updatedb.csh ${DESTDIR}/usr/libexec/locate.updatedb

.include "../../Makefile.inc"
.include <bsd.prog.mk>
