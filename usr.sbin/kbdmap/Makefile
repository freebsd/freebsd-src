# Makefile for kbdmap / vidfont
# $Id: Makefile,v 1.1.1.1 1995/03/24 00:52:28 jkh Exp $

SCRIPT=	${.CURDIR}/kbdmap.pl
LINKS=	${DESTDIR}${BINDIR}/kbdmap ${DESTDIR}${BINDIR}/vidfont

all:
	@echo -n

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/kbdmap

.include <bsd.prog.mk>
