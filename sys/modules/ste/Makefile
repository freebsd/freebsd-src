# $FreeBSD: src/sys/modules/ste/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_ste
SRCS=	if_ste.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
