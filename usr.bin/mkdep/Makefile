#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAN=	mkdep.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}${BINDIR}/mkdep

.include <bsd.prog.mk>
