# $FreeBSD: src/etc/periodic/Makefile.inc,v 1.3.2.1 1999/08/29 14:19:33 peter Exp $

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
