#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	locate

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/updatedb.sh ${DESTDIR}/usr/libexec/locate.updatedb

.include "../../Makefile.inc"
.include <bsd.prog.mk>
