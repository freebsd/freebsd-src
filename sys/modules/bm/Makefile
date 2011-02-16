# $FreeBSD: src/sys/modules/bm/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bm

KMOD=	if_bm
SRCS=	if_bm.c
SRCS+=	bus_if.h device_if.h miibus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
