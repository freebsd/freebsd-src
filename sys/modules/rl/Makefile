# $FreeBSD: src/sys/modules/rl/Makefile,v 1.11.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
