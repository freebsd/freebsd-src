# $FreeBSD: src/sys/modules/pcn/Makefile,v 1.3 2005/09/27 18:10:37 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_pcn
SRCS=	if_pcn.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
