# $FreeBSD: src/sys/modules/stge/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
