#	@(#)Makefile	5.1 (Berkeley) 5/28/90
#	$Id: Makefile,v 1.3 1995/05/03 03:26:53 rgrimes Exp $

MAN8=	named.restart.8
CLEANFILES+=named.restart

PIDDIR=/var/run
INDOT=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    named.restart ${DESTDIR}${BINDIR}

all: named.restart

named.restart: named.restart.sh Makefile
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${BINDIR}|" \
	    < ${.CURDIR}/named.restart.sh > named.restart

.include "${.CURDIR}/../../../Makefile.inc"
.include <bsd.prog.mk>
