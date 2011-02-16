# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.6.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
