# $FreeBSD: src/sys/modules/tl/Makefile,v 1.15 2001/01/06 14:00:31 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_tl
SRCS=	if_tl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
