# $FreeBSD: src/sys/modules/geom/geom_part/geom_part_bsd/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_bsd
SRCS=	g_part_bsd.c

SRCS+=	bus_if.h device_if.h g_part_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
