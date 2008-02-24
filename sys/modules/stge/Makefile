# $FreeBSD: src/sys/modules/stge/Makefile,v 1.1 2006/07/25 00:45:55 yongari Exp $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
