#	@(#)Makefile	8.2 (Berkeley) 4/22/94

MAN8=	nologin.8

nologin clean depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}/sbin/nologin

.include <bsd.prog.mk>
