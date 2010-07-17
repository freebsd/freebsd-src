# $FreeBSD: src/sys/modules/stge/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
