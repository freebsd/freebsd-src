# $FreeBSD: src/sys/modules/msk/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/msk

KMOD=	if_msk
SRCS=	if_msk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
