# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.2.2.1 2001/12/21 17:42:35 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NOMAN=	yes
SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
