#	@(#)Makefile	8.2 (Berkeley) 4/22/94

MAN8=	nologin.0

nologin clean depend lint tags:

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}/sbin/nologin

cleandir:
	rm -f nologin.0

.include <bsd.prog.mk>
