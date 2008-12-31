# $FreeBSD: src/usr.sbin/diskinfo/Makefile,v 1.3.26.1 2008/11/25 02:59:29 kensmith Exp $

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
