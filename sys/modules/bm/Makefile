# $FreeBSD: src/sys/modules/bm/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bm

KMOD=	if_bm
SRCS=	if_bm.c
SRCS+=	bus_if.h device_if.h miibus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>
