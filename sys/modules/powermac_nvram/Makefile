# $FreeBSD: src/sys/modules/powermac_nvram/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/powermac_nvram

KMOD=	powermac_nvram
SRCS=	powermac_nvram.c powermac_nvramvar.h
SRCS+=	bus_if.h device_if.h
SRCS+=	ofw_bus_if.h

.include <bsd.kmod.mk>
