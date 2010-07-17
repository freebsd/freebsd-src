# $FreeBSD: src/sys/modules/pcn/Makefile,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pcn

KMOD=	if_pcn
SRCS=	if_pcn.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
