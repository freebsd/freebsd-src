# $FreeBSD: src/etc/periodic/Makefile.inc,v 1.4 1999/08/27 23:24:02 peter Exp $

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
