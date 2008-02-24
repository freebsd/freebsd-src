# $FreeBSD: src/sys/modules/ste/Makefile,v 1.12 2005/09/27 18:10:38 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_ste
SRCS=	if_ste.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
