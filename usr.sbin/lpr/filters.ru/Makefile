PROG=   koi2alt
NOMAN=	noman
BINDIR= /usr/libexec/lpr/ru

SAMPLES=bjc-240.sh.sample

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SAMPLES} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
