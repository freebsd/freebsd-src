#	$Id: Makefile,v 1.1.1.1.4.1 1995/09/17 23:42:31 davidg Exp $

FILES=	linuxdoc.dtd linuxdoc.dcl linuxdoc.sty
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>
