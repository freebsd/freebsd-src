# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.6 2004/02/23 20:13:51 johan Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	0
MAN=	fdisk.8

CFLAGS	+= -DPC98

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>
