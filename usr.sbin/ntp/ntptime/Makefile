# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.3 2001/07/20 06:20:05 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NOMAN=	yes
SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
