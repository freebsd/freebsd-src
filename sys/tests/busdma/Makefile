# $FreeBSD$

.PATH: ${.CURDIR}/../../tests/busdma

KMOD=	busdma
SRCS=	busdma.c

SRCS+=	bus_if.h device_if.h

MFILES= kern/bus_if.m kern/device_if.m

.include <bsd.kmod.mk>
