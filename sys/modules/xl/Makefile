# $FreeBSD: src/sys/modules/xl/Makefile,v 1.14.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_xl
SRCS=	if_xl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
