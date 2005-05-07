# $FreeBSD: src/sbin/fdisk/Makefile,v 1.11 2004/02/23 20:13:51 johan Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
