#	@(#)Makefile	5.1 (Berkeley) 5/23/90

MAN1=	shar.1

all shar: ${MAN1}

clean depend lint tags:

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
