#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/xinstall/Makefile,v 1.11.2.1 2000/06/28 02:33:16 joe Exp $

PROG=	xinstall
SRCS=	xinstall.c
MAN1=	install.1

install: maninstall
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
