# $FreeBSD$

KMOD=	splash_bmp
SRCS= 	splash_bmp.c

NOMAN=

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
