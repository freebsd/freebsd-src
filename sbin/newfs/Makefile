#	@(#)Makefile	8.2 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/newfs/Makefile,v 1.22.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	newfs
DPADD=	${LIBUFS}
LDADD=	-lufs
SRCS=	newfs.c mkfs.c
WARNS?=	2
MAN=	newfs.8

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest01.sh 
	sh ${.CURDIR}/runtest00.sh | tee _.test
	diff --ignore-matching-lines=FreeBSD _.test ${.CURDIR}/ref.test
	echo All Tests Passed
