#       @(#)Makefile    8.1 (Berkeley) 6/5/93

NOOBJ=  noobj

all clean cleandir depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 dir-tmpl ${DESTDIR}${BINDIR}/info/dir

.include <bsd.prog.mk>
