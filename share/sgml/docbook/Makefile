#	$Id$

FILES=	calstbl.mod dbgenent.mod dbhier.mod dbpool.mod
FILES+=	docbook.dcl docbook.dtd
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>


