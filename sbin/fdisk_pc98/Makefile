# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.7.2.1.10.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>
