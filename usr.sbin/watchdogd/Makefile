# $FreeBSD: src/usr.sbin/watchdogd/Makefile,v 1.5.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm -lutil
DPADD=	${LIBM} ${LIBUTIL}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
