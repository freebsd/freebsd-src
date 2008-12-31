# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
