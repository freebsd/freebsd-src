# $FreeBSD: src/sys/modules/bios/smapi/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
