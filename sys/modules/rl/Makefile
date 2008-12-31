# $FreeBSD: src/sys/modules/rl/Makefile,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
