# $FreeBSD$

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
