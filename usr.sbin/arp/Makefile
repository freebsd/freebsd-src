#	@(#)Makefile	8.2 (Berkeley) 4/18/94

PROG=	arp
MAN8=	arp.0
CLEANFILES=arp4.0

all: ${PROG} arp4.0 ${MAN8}

beforeinstall:
	install -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} arp4.0 \
	    ${DESTDIR}${MANDIR}4/arp.0

.include <bsd.prog.mk>
