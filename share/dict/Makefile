#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

# XXX MISSING:	connectives words
FILES=	README propernames web2 web2a
SYMLINKS= web2 ${BINDIR}/dict/words
NOOBJ=	noobj

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/dict

.include <bsd.prog.mk>
