#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.1.1.1 1999/08/08 23:31:13 itojun Exp $
#	$FreeBSD$

MAN=	prefix.8

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/prefix.sh ${DESTDIR}${BINDIR}/prefix

.include <bsd.prog.mk>
