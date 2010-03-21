# $FreeBSD: src/sys/modules/bm/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bm

KMOD=	if_bm
SRCS=	if_bm.c
SRCS+=	bus_if.h device_if.h miibus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
