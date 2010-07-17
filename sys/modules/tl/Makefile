# $FreeBSD: src/sys/modules/tl/Makefile,v 1.17.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/tl

KMOD=	if_tl
SRCS=	if_tl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
