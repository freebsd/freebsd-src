#	@(#)Makefile	8.2 (Berkeley) 4/3/94

MAN1=	pagesize.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pagesize.sh ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
