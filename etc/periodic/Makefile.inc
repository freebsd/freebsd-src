#	$Id: Makefile.inc,v 1.1 1997/08/12 18:23:25 pst Exp $

BINDIR= /etc/periodic/${.CURDIR:T}
BINOWN=	 root
BINGRP=	 wheel

all depend etc lint:

install:
	cd ${.CURDIR}; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
		${DESTDIR}/${BINDIR} 
