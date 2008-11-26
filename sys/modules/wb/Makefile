# $FreeBSD: src/sys/modules/wb/Makefile,v 1.12.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c opt_bdg.h device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
