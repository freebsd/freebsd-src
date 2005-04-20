# $FreeBSD$

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm
DPADD=	${LIBM}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
