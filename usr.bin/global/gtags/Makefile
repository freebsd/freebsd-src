#	@(#)Makefile	1.0 (Berkeley) 4/21/96

MAN1=	gtags.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/gtags.sh ${DESTDIR}/${BINDIR}/gtags

.include <bsd.prog.mk>
