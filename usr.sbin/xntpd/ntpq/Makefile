#
# $FreeBSD: src/usr.sbin/xntpd/ntpq/Makefile,v 1.10.2.1 1999/08/29 15:49:25 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntpq
MAN8= ${.CURDIR}/../doc/ntpq.8
CLEANFILES+= .version version.c
BINDIR= /usr/bin

SRCS= ntpq.c ntpq_ops.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpq

.include <bsd.prog.mk>
