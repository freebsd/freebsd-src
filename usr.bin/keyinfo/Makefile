#	@(#)Makefile	5.5 (Berkeley) 7/1/90

MAN1=	keyinfo.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/keyinfo.sh ${DESTDIR}${BINDIR}/keyinfo

.include <bsd.prog.mk>
