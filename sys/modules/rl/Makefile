# $FreeBSD: src/sys/modules/rl/Makefile,v 1.10.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
