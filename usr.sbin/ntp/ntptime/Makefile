# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.6.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
