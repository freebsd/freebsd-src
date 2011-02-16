# $FreeBSD: src/sys/modules/wb/Makefile,v 1.13.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
