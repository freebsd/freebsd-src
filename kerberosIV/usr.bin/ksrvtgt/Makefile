# $FreeBSD$

SCRIPTS=	ksrvtgt
MAN=		${KRB4DIR}/man/ksrvtgt.1
CLEANFILES=	ksrvtgt

ksrvtgt:	ksrvtgt.in
	sed 's!%bindir%!${BINDIR}!' ${KRB4DIR}/appl/kauth/ksrvtgt.in > $@

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kauth
