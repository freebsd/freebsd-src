# Makefile for kbdmap / vidfont
# $Id: Makefile,v 1.2 1995/03/24 04:36:19 jkh Exp $

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
