# $FreeBSD: src/sys/modules/dc/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/dc

KMOD=	if_dc
SRCS=	if_dc.c dcphy.c pnphy.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
