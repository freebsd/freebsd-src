# $FreeBSD: src/sys/modules/stge/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
