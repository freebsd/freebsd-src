#       @(#)Makefile    5.6 (Berkeley) 6/23/90

NOMAN=	noman
CLEANFILES= makewhatis

beforeinstall: makewhatis
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/makewhatis.sh ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>

makewhatis: makewhatis.sh
	sed -e 's/%sections%/ "1", "n", "l", "6", "8", "2", "3", "4", "5", "7", "p", "o", NULL/' \
	   ${.CURDIR}/makewhatis.sh > makewhatis

