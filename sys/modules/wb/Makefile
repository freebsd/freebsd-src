# $FreeBSD: src/sys/modules/wb/Makefile,v 1.13.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
