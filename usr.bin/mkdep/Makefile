#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN1=	mkdep.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
