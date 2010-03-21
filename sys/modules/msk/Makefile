# $FreeBSD: src/sys/modules/msk/Makefile,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/msk

KMOD=	if_msk
SRCS=	if_msk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
