#	$Id: Makefile.inc,v 1.3 1998/09/19 22:42:02 obrien Exp $

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
