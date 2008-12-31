# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>
