#	@(#)Makefile	5.5 (Berkeley) 7/1/90

BINOWN= root
BINMODE=4555

MAN1=	keyinfo.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/keyinfo.pl ${DESTDIR}${BINDIR}/keyinfo

.include <bsd.prog.mk>
