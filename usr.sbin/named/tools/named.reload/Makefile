#	@(#)Makefile	5.1 (Berkeley) 5/28/90

MAN8=	named.reload.8
CLEANFILES+=named.reload

PIDDIR=/var/run
INDOT=
DESTSBIN=${DESTDIR}${BINDIR}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    named.reload ${DESTDIR}${BINDIR}

all: named.reload

named.reload: named.reload.sh Makefile
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    < ${.CURDIR}/named.reload.sh > named.reload

.include "${.CURDIR}/../../../Makefile.inc"
.include <bsd.prog.mk>
