#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	xinstall
SRCS=	setflags.c xinstall.c
.PATH:	${.CURDIR}/../../lib/libc/gen
MAN1=	install.1

install: maninstall
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
