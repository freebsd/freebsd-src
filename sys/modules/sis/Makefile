# $FreeBSD: src/sys/modules/sis/Makefile,v 1.12.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sis

KMOD=	if_sis
SRCS=	if_sis.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
