#	@(#)Makefile	8.2 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/newfs/Makefile,v 1.22.28.1 2008/10/02 02:57:24 kensmith Exp $

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
