#	$Id:$

FILES=	linuxdoc linuxdoc.dec
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dtd

.include <bsd.prog.mk>
