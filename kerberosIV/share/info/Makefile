#       @(#)Makefile    8.1 (Berkeley) 6/5/93
# $FreeBSD$

NOOBJ=  noobj

all clean cleandir depend lint tags:

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 dir-tmpl \
		${DESTDIR}${BINDIR}/info/dir-tmpl
	if [ ! -f ${DESTDIR}${BINDIR}/info/dir ]; then \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 dir-tmpl \
			${DESTDIR}${BINDIR}/info/dir ; \
	fi

.include <bsd.prog.mk>
