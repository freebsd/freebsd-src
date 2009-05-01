# $FreeBSD: src/sys/modules/pcn/Makefile,v 1.3.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pcn

KMOD=	if_pcn
SRCS=	if_pcn.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
