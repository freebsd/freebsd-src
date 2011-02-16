# $FreeBSD: src/sys/modules/wb/Makefile,v 1.14.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/wb

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
