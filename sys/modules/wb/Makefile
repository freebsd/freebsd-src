# $FreeBSD: src/sys/modules/wb/Makefile,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
