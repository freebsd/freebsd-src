# $FreeBSD$

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
