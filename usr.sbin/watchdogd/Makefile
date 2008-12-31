# $FreeBSD: src/usr.sbin/watchdogd/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm -lutil
DPADD=	${LIBM} ${LIBUTIL}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
