# Makefile for kbdmap / vidfont
# $FreeBSD: src/usr.sbin/kbdmap/Makefile,v 1.9 1999/08/28 01:16:38 peter Exp $

MAINTAINER= wosch


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
