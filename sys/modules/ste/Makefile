# $FreeBSD: src/sys/modules/ste/Makefile,v 1.12.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ste

KMOD=	if_ste
SRCS=	if_ste.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
