# $FreeBSD: src/sys/modules/xl/Makefile,v 1.15.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/xl

KMOD=	if_xl
SRCS=	if_xl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
