#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/util

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntptime
NOMAN= yes

SRCS= 	ntptime.c

.include <bsd.prog.mk>
