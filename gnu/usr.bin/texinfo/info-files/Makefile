#
# Makefile for INFO files
#

INFOFILES= dir
NOOBJ=	noobj

install:
	${INSTALL} -c -g ${BINGRP} -o ${BINOWN} -m 444 ${INFOFILES} \
		${DESTDIR}${INFODIR}

.include <bsd.prog.mk>
