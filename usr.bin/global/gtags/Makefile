#	@(#)Makefile	1.0 (Berkeley) 4/21/96

MAN1=	gtags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/gtags.sh ${DESTDIR}/usr/bin/gtags

.include <bsd.prog.mk>
