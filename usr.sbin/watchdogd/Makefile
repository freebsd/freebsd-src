# $FreeBSD: src/usr.sbin/watchdogd/Makefile,v 1.4 2004/02/28 20:56:34 phk Exp $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm
DPADD=	${LIBM}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
