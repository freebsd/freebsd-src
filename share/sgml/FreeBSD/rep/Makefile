#	$Id: Makefile,v 1.1.1.1.4.1 1995/09/17 23:42:40 davidg Exp $

FILES=  html.general html.mapping html.math latex.general latex.mapping
FILES+= latex.math nroff.general nroff.mapping nroff.math
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/rep

.include <bsd.prog.mk>
