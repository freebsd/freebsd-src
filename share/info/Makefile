#       @(#)Makefile    8.1 (Berkeley) 6/5/93

NOOBJ=  noobj

DISTRIBUTION=	info

all clean cleandir depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 dir-tmpl \
		${DESTDIR}${BINDIR}/info/dir-tmpl
	if [ ! -f ${DESTDIR}${BINDIR}/info/dir ]; then \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 dir-tmpl \
			${DESTDIR}${BINDIR}/info/dir ; \
	fi

.include <bsd.prog.mk>
