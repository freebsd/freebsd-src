#	$Id: Makefile,v 1.1.1.1 1995/05/09 23:19:46 jfieber Exp $

FILES=	linuxdoc linuxdoc.dec
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dtd

.include <bsd.prog.mk>
