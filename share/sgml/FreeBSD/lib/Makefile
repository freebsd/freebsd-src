#	$Id: Makefile,v 1.2 1995/07/25 00:34:47 bde Exp $

FILES=	linuxdoc.sty
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/lib

.include <bsd.prog.mk>
