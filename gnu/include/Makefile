#	$Id: Makefile,v 1.3 1996/08/30 03:12:16 peter Exp $
#
all depend lint tags:

NOOBJ=	noobj

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/values.h \
		${DESTDIR}/usr/include

.include <bsd.prog.mk>
