#	$Id: Makefile.inc,v 1.2 1998/05/31 11:38:25 bde Exp $

BINDIR=	/etc/periodic/${.CURDIR:T}
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
