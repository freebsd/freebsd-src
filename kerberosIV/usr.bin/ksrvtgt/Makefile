# $FreeBSD$

SCRIPTS=	ksrvtgt.in
MAN=		${KRB4DIR}/man/ksrvtgt.1

all:		ksrvtgt

CLEANFILES+=	ksrvtgt

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ksrvtgt \
	    ${DESTDIR}${BINDIR}/ksrvtgt

ksrvtgt:	ksrvtgt.in
	sed 's!%bindir%!${BINDIR}!' ${KRB4DIR}/appl/kauth/ksrvtgt.in > $@

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kauth
