#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/dict/Makefile,v 1.9 1999/12/26 10:31:29 bde Exp $

# XXX MISSING:	connectives words
FILES=	README propernames web2 web2a
SYMLINKS= web2 ${BINDIR}/dict/words
NOOBJ=	noobj

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/dict

.include <bsd.prog.mk>
