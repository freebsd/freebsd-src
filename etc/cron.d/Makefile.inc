#	$Id$

BINDIR= /etc/cron.d/${.CURDIR:T}
BINOWN=	 root
BINGRP=	 wheel

all depend etc lint:

install:
	cd ${.CURDIR}; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN} \
		${DESTDIR}/${BINDIR} 
