# $FreeBSD: src/sys/modules/splash/bmp/Makefile,v 1.2.2.1 2000/08/13 10:59:00 nyan Exp $

KMOD=	splash_bmp
SRCS= 	splash_bmp.c

NOMAN=

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
