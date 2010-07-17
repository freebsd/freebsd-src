# $FreeBSD: src/usr.sbin/diskinfo/Makefile,v 1.3.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	diskinfo
MAN=	diskinfo.8

WARNS?=	5

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad4 md50
	./${PROG} -v /dev/ad4 md50
	./${PROG} -t /dev/ad4
