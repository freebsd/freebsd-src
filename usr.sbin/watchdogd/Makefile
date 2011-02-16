# $FreeBSD: src/usr.sbin/watchdogd/Makefile,v 1.5.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm -lutil
DPADD=	${LIBM} ${LIBUTIL}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
