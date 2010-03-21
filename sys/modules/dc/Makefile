# $FreeBSD: src/sys/modules/dc/Makefile,v 1.8.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/dc

KMOD=	if_dc
SRCS=	if_dc.c dcphy.c pnphy.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
