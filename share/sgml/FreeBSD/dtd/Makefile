#	$Id: Makefile,v 1.2 1995/07/25 00:34:37 bde Exp $

FILES=	linuxdoc linuxdoc.dec
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dtd

.include <bsd.prog.mk>
