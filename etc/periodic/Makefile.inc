#	$Id: Makefile.inc,v 1.1.1.1 1997/08/16 17:04:00 pst Exp $

BINDIR=	/etc/periodic/${.CURDIR:T}
BINOWN=	root
BINGRP=	wheel
NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
	    ${DESTDIR}${BINDIR} 
