# $FreeBSD: src/gnu/include/Makefile,v 1.8 1999/08/27 23:32:36 peter Exp $

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
