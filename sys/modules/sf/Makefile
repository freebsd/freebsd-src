# $FreeBSD: src/sys/modules/sf/Makefile,v 1.14.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sf

KMOD=	if_sf
SRCS=	if_sf.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
