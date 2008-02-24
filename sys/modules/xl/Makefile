# $FreeBSD: src/sys/modules/xl/Makefile,v 1.14 2005/09/27 18:10:41 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_xl
SRCS=	if_xl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
