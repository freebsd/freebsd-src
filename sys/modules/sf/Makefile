# $FreeBSD: src/sys/modules/sf/Makefile,v 1.14 2005/09/27 18:10:38 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_sf
SRCS=	if_sf.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
