# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.6.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
