# $FreeBSD: src/usr.sbin/watchdogd/Makefile,v 1.4.8.1 2006/01/15 17:50:38 delphij Exp $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8
WARNS?=	6

LDADD=	-lm -lutil
DPADD=	${LIBM} ${LIBUTIL}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
