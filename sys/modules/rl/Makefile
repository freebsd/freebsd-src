# $FreeBSD: src/sys/modules/rl/Makefile,v 1.10 2001/01/06 14:00:13 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
