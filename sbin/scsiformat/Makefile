#	@(#)Makefile	5.3 (Berkeley) 6/5/93

MAN8=	scsiformat.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/scsiformat.sh ${DESTDIR}${BINDIR}/scsiformat

.include <bsd.prog.mk>
