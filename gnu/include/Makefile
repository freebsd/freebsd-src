#	$Id: Makefile,v 1.7 1997/02/22 15:42:31 peter Exp $

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
