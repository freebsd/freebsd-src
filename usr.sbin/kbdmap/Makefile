# Makefile for kbdmap / vidfont
# $FreeBSD$

MAINTAINER= wosch


SCRIPT=	${.CURDIR}/kbdmap.pl
LINKS=	${BINDIR}/kbdmap ${BINDIR}/vidfont
MAN=   kbdmap.1
MLINKS= kbdmap.1 vidfont.1

all:
	@echo -n

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/kbdmap

.include <bsd.prog.mk>
