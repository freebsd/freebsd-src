# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
