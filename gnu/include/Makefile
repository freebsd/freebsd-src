#	$Id: Makefile,v 1.5 1996/08/30 21:57:39 peter Exp $

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
