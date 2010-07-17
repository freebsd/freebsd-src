# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.8.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>
