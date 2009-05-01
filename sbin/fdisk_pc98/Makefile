# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.8.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>
