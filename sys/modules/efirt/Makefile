# $FreeBSD$

.PATH: ${.CURDIR}/../../${MACHINE}/${MACHINE}
.PATH: ${.CURDIR}/../../dev/efidev

KMOD=	efirt
SRCS=	efirt.c efidev.c
SRCS+=  device_if.h bus_if.h

.include <bsd.kmod.mk>
