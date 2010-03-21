# $FreeBSD: src/sys/modules/tl/Makefile,v 1.16.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_tl
SRCS=	if_tl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
