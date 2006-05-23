# $FreeBSD: src/sys/modules/auxio/Makefile,v 1.1.2.2 2005/02/28 05:18:32 yongari Exp $

.PATH: ${.CURDIR}/../../dev/auxio

KMOD=	auxio

SRCS=	auxio.c
SRCS+=  device_if.h bus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
