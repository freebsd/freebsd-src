# $FreeBSD: src/share/doc/IPv6/Makefile,v 1.1 2000/03/02 10:34:11 shin Exp $

FILES=  IMPLEMENTATION

DOCDIR = ${SHAREDIR}/doc/IPv6

NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
		${DESTDIR}${DOCDIR}

.include <bsd.prog.mk>
