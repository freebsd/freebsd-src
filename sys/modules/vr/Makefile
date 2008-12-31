# $FreeBSD: src/sys/modules/vr/Makefile,v 1.13.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/vr

KMOD=	if_vr
SRCS=	if_vr.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
