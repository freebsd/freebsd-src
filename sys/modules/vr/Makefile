# $FreeBSD: src/sys/modules/vr/Makefile,v 1.13 2005/09/27 18:10:40 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_vr
SRCS=	if_vr.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
