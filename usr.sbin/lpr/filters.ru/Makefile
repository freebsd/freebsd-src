# $FreeBSD$

SUBDIR = koi2alt koi2855

SAMPLES=bjc-240.sh.sample

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SAMPLES} ${DESTDIR}${BINDIR}

.include "Makefile.inc"
.include <bsd.prog.mk>

