# $FreeBSD: src/sys/modules/sf/Makefile,v 1.14.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sf

KMOD=	if_sf
SRCS=	if_sf.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
