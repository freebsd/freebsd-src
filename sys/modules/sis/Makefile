# $FreeBSD: src/sys/modules/sis/Makefile,v 1.11.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sis

KMOD=	if_sis
SRCS=	if_sis.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
