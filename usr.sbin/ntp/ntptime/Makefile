#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/util

PROG= ntptime

SRCS= 	ntptime.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
