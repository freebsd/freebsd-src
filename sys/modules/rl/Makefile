# $FreeBSD: src/sys/modules/rl/Makefile,v 1.13.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
