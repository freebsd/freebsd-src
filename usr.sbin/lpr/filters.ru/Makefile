# $FreeBSD: src/usr.sbin/lpr/filters.ru/Makefile,v 1.5 1999/09/21 20:42:05 ache Exp $

SUBDIR = koi2alt koi2855

SAMPLES=bjc-240.sh.sample

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SAMPLES} ${DESTDIR}${BINDIR}

.include "Makefile.inc"
.include <bsd.prog.mk>

