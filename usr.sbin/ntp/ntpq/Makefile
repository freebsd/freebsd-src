#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpq

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntpq
NOMAN= yes
CLEANFILES+= .version version.c
BINDIR= /usr/bin

SRCS= 	ntpq.c ntpq_ops.c version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
