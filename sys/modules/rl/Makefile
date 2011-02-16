# $FreeBSD: src/sys/modules/rl/Makefile,v 1.11.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
