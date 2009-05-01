# $FreeBSD: src/sys/modules/wb/Makefile,v 1.13.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
