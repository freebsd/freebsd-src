# Makefile for kbdmap / vidfont
# Makefile,v 1.3 1995/04/06 14:41:50 ache Exp

SCRIPT=	${.CURDIR}/kbdmap.pl
LINKS=	${DESTDIR}${BINDIR}/kbdmap ${DESTDIR}${BINDIR}/vidfont
MAN1=   kbdmap.1
MLINKS= kbdmap.1 vidfont.1

all:
	@echo -n

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/kbdmap

.include <bsd.prog.mk>
