# $FreeBSD$

MAN=		${KRB4DIR}/man/ksrvtgt.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${KRB4DIR}/appl/kauth/ksrvtgt.in ${DESTDIR}${BINDIR}/ksrvtgt

depend all:

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kauth
