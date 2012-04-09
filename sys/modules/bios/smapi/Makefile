# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
