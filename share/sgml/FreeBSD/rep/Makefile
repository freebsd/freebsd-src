#	$Id:$

FILES=  html.general html.mapping html.math latex.general latex.mapping
FILES+= latex.math nroff.general nroff.mapping nroff.math
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/rep

.include <bsd.prog.mk>
