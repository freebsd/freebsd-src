# $FreeBSD: src/sys/modules/msk/Makefile,v 1.1 2006/12/13 02:33:33 yongari Exp $

.PATH: ${.CURDIR}/../../dev/msk

KMOD=	if_msk
SRCS=	if_msk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
