# $FreeBSD: src/sys/modules/ste/Makefile,v 1.11 2001/01/06 14:00:23 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_ste
SRCS=	if_ste.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
