# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
