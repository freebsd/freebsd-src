# $FreeBSD: src/sbin/fdisk/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
