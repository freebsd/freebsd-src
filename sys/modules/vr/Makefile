# $FreeBSD: src/sys/modules/vr/Makefile,v 1.14.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/vr

KMOD=	if_vr
SRCS=	if_vr.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
