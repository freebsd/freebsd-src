# Makefile for kbdmap / vidfont
# $Id: Makefile,v 1.5 1995/07/25 00:37:15 bde Exp $

SCRIPT=	${.CURDIR}/kbdmap.pl
LINKS=	${BINDIR}/kbdmap ${BINDIR}/vidfont
MAN1=   kbdmap.1
MLINKS= kbdmap.1 vidfont.1

all:
	@echo -n

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/kbdmap

.include <bsd.prog.mk>
