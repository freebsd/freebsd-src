# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.3.8.1 2005/02/13 07:23:44 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NOMAN=
SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
