# $FreeBSD: src/sbin/fdisk_pc98/Makefile,v 1.5 2003/05/01 13:53:41 nyan Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS=	0
MAN=	fdisk.8

CFLAGS	+= -DPC98

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>
