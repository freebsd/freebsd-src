#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN1=	shar.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
