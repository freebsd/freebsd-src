# $FreeBSD: src/sys/modules/auxio/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/auxio

KMOD=	auxio

SRCS=	auxio.c
SRCS+=  device_if.h bus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
