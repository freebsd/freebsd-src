# $FreeBSD: src/sys/modules/auxio/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/auxio

KMOD=	auxio

SRCS=	auxio.c
SRCS+=  device_if.h bus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
