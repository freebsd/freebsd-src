# $FreeBSD: src/sys/modules/auxio/Makefile,v 1.2 2005/02/25 06:59:56 yongari Exp $

.PATH: ${.CURDIR}/../../dev/auxio

KMOD=	auxio

SRCS=	auxio.c
SRCS+=  device_if.h bus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
