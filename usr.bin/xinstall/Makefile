#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	xinstall
MAN=	install.1

install: maninstall
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
