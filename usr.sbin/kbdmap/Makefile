# Makefile for kbdmap / vidfont
# $FreeBSD: src/usr.sbin/kbdmap/Makefile,v 1.7.2.1 1999/08/29 15:42:40 peter Exp $

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
