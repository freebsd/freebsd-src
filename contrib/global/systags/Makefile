#	@(#)Makefile	1.0 (Berkeley) 4/21/96

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/systags.sh ${DESTDIR}/usr/bin/systags

.include <bsd.prog.mk>
