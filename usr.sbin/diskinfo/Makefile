# $FreeBSD: src/usr.sbin/diskinfo/Makefile,v 1.1 2003/04/09 10:52:10 phk Exp $

PROG=	diskinfo
MAN=	diskinfo.8

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad4 md50
	./${PROG} -v /dev/ad4 md50
	./${PROG} -t /dev/ad4
