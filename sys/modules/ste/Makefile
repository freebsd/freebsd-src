# $FreeBSD: src/sys/modules/ste/Makefile,v 1.13.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ste

KMOD=	if_ste
SRCS=	if_ste.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
