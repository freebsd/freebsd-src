#
# $FreeBSD: src/usr.sbin/ntp/ntpq/Makefile,v 1.2 2000/01/01 23:58:00 roberto Exp $
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpq

PROG= ntpq
BINDIR= /usr/bin

SRCS= 	ntpq.c ntpq_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
