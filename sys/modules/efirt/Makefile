# $FreeBSD$

.PATH: ${SRCTOP}/sys/${MACHINE}/${MACHINE}
.PATH: ${SRCTOP}/sys/dev/efidev

KMOD=	efirt
SRCS=	efirt.c efidev.c
SRCS+=  device_if.h bus_if.h

.include <bsd.kmod.mk>
