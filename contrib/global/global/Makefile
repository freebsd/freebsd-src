#	@(#)Makefile	1.0 (Berkeley) 4/21/96

MAN1=	global.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/global.pl ${DESTDIR}/usr/bin/global

.include <bsd.prog.mk>
