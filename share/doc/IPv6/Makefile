# $FreeBSD$

FILES=  IMPLEMENTATION

DOCDIR = ${SHAREDIR}/doc/IPv6

NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
		${DESTDIR}${DOCDIR}

.include <bsd.prog.mk>
