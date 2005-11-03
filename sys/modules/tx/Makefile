# $FreeBSD: src/sys/modules/tx/Makefile,v 1.9 2003/01/19 02:59:33 obrien Exp $

.PATH: ${.CURDIR}/../../dev/tx

KMOD=	if_tx
SRCS=	if_tx.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
