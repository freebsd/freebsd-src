# $FreeBSD: src/sbin/fdisk/Makefile,v 1.10 2003/04/13 21:34:16 phk Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
