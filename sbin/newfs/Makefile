#	@(#)Makefile	8.2 (Berkeley) 3/27/94
# $FreeBSD$

.PATH: ${.CURDIR}/../../sys/geom

PROG=	newfs
DPADD=	${LIBUFS}
LDADD=	-lufs
SRCS=	newfs.c mkfs.c geom_bsd_enc.c

WARNS?=	2
MAN=	newfs.8

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest01.sh 
	sh ${.CURDIR}/runtest00.sh | tee _.test
	diff --ignore-matching-lines=FreeBSD _.test ${.CURDIR}/ref.test
	echo All Tests Passed
