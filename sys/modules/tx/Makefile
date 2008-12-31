# $FreeBSD: src/sys/modules/tx/Makefile,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/tx

KMOD=	if_tx
SRCS=	if_tx.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
