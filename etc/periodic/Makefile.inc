# $FreeBSD$

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
