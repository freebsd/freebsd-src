# $FreeBSD$

.PATH:  ${.CURDIR}/../../i386/smapi

KMOD=	smapi
SRCS=	smapi_isa.c smapi.c smapi_bios.S \
	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>
