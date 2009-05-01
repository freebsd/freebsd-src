# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
