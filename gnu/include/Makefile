# $FreeBSD: src/gnu/include/Makefile,v 1.7.2.1 1999/08/29 14:28:30 peter Exp $

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
