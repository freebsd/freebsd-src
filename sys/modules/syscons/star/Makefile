# $FreeBSD: src/sys/modules/syscons/star/Makefile,v 1.9.2.1 2000/05/09 13:42:17 nyan Exp $

KMOD=	star_saver
SRCS= 	star_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
