#	$Id: Makefile,v 1.1.1.1.4.1 1995/09/17 23:42:39 davidg Exp $

FILES=	linuxdoc.sty
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/lib

.include <bsd.prog.mk>
