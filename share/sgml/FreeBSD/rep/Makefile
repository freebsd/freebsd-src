#	$Id: Makefile,v 1.2 1995/07/25 00:34:52 bde Exp $

FILES=  html.general html.mapping html.math latex.general latex.mapping
FILES+= latex.math nroff.general nroff.mapping nroff.math
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/rep

.include <bsd.prog.mk>
