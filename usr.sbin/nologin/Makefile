#	@(#)Makefile	8.2 (Berkeley) 4/22/94

MAN8=	nologin.8
MAN5=	nologin.5

nologin depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}${BINDIR}/nologin

.include <bsd.prog.mk>
