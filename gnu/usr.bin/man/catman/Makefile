#	$Id

MAINTAINER= wosch


MAN1=	catman.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/catman.perl ${DESTDIR}${BINDIR}/catman

.include <bsd.prog.mk>
