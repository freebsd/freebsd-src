# $FreeBSD: src/sys/modules/tx/Makefile,v 1.8 2002/04/19 22:43:48 semenu Exp $

.PATH: ${.CURDIR}/../../dev/tx

KMOD=	if_tx
SRCS=	if_tx.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
