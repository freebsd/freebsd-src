# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/util

PROG=	ntptime
NOMAN=
SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
