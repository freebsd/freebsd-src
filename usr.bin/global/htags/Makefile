#	@(#)Makefile	1.0 (Berkeley) 4/21/96

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/htags.pl ${DESTDIR}/usr/bin/htags

.include <bsd.prog.mk>
