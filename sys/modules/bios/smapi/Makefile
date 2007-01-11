# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2 2005/01/26 16:24:55 imp Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
