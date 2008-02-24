# $FreeBSD: src/sys/modules/tl/Makefile,v 1.16 2005/09/27 18:10:39 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_tl
SRCS=	if_tl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
