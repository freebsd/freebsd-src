#	@(#)Makefile	5.1 (Berkeley) 5/28/90

MAN8=	named.restart.8
CLEANFILES+=named.restart

PIDDIR=/var/run
INDOT=

beforeinstall:	named.restart
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${.CURDIR}/named.restart ${DESTDIR}${BINDIR}

named.restart: named.restart.sh Makefile
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${BINDIR}|" \
	    < named.restart.sh > named.restart

.include "${.CURDIR}/../../../Makefile.inc"
.include <bsd.prog.mk>
