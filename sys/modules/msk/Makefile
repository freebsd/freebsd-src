# $FreeBSD: src/sys/modules/msk/Makefile,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/msk

KMOD=	if_msk
SRCS=	if_msk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
