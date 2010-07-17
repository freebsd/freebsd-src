# $FreeBSD: src/sys/modules/tx/Makefile,v 1.10.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/tx

KMOD=	if_tx
SRCS=	if_tx.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
