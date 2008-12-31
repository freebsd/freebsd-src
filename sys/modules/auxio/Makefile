# $FreeBSD: src/sys/modules/auxio/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/auxio

KMOD=	auxio

SRCS=	auxio.c
SRCS+=  device_if.h bus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
