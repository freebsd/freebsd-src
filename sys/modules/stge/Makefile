# $FreeBSD: src/sys/modules/stge/Makefile,v 1.1.2.1 2006/08/08 09:09:18 yongari Exp $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
