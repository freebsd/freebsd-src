# $FreeBSD: src/sys/modules/syscons/snake/Makefile,v 1.9.2.1 2000/05/09 13:42:14 nyan Exp $

KMOD=	snake_saver
SRCS= 	snake_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
