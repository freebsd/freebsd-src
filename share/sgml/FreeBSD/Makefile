#	$FreeBSD$

FILES=	linuxdoc.dtd linuxdoc.dcl linuxdoc.sty
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

afterdistribute:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DISTDIR}/${DISTRIBUTION}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>
