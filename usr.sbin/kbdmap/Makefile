# Makefile for kbdmap / vidfont
# $Id: Makefile,v 1.3.4.1 1995/09/20 07:30:33 davidg Exp $

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
