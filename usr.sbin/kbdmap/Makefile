# Makefile for kbdmap / kbdfont
# $Id$

SCRIPT=	${.CURDIR}/kbdmap.pl
LINKS=	${DESTDIR}${BINDIR}/kbdmap ${DESTDIR}${BINDIR}/kbdfont

all:
	@echo -n

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/kbdmap

.include <bsd.prog.mk>
