# $FreeBSD: src/sys/modules/wb/Makefile,v 1.13 2005/09/27 18:10:41 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
