#	$Id: Makefile,v 1.1.1.1.4.1 1995/09/17 23:42:31 davidg Exp $

FILES=	linuxdoc linuxdoc.dec
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dtd

.include <bsd.prog.mk>
