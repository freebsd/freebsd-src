# $FreeBSD$

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	0
MAN=	fdisk.8

CFLAGS	+= -DPC98

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>
