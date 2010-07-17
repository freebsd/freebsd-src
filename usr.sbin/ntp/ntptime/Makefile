# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.6.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
