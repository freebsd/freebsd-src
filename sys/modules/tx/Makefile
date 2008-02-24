# $FreeBSD: src/sys/modules/tx/Makefile,v 1.10 2005/09/27 18:10:39 mlaier Exp $

.PATH: ${.CURDIR}/../../dev/tx

KMOD=	if_tx
SRCS=	if_tx.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
