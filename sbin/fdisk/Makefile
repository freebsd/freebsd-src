# $FreeBSD$

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
