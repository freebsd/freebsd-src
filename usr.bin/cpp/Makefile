#	@(#)Makefile	8.1 (Berkeley) 7/9/93

NOMAN=noman
NOOBJ=noobj

all nologin clean cleandir depend lint tags:

beforeinstall:
.if ${MACHINE} == "sparc"
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/cpp.notraditional.sh ${DESTDIR}/usr/bin/cpp
.else
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/cpp.sh ${DESTDIR}/usr/bin/cpp
.endif

.include <bsd.prog.mk>
