# $FreeBSD: src/sys/modules/rl/Makefile,v 1.11 2005/09/27 18:10:37 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
