#	@(#)Makefile	5.1 (Berkeley) 5/28/90

MAN8=	named.reload.8
CLEANFILES+=named.reload

PIDDIR=/var/run
INDOT=

beforeinstall:	named.reload
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${.CURDIR}/named.reload ${DESTDIR}${BINDIR}

named.reload: named.reload.sh Makefile
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    < named.reload.sh > named.reload

.include "${.CURDIR}/../../../Makefile.inc"
.include <bsd.prog.mk>
