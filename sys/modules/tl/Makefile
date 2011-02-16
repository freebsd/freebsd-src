# $FreeBSD: src/sys/modules/tl/Makefile,v 1.16.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_tl
SRCS=	if_tl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
