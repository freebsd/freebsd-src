# $FreeBSD$

KMOD=	splash_bmp
SRCS= 	splash_bmp.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
