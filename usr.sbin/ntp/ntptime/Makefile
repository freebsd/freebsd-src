#
# $FreeBSD: src/usr.sbin/ntp/ntptime/Makefile,v 1.2 2000/01/01 23:58:00 roberto Exp $
#

.PATH:	${.CURDIR}/../../../contrib/ntp/util

PROG= ntptime

SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
