# $FreeBSD: src/sys/modules/xl/Makefile,v 1.13 2001/01/06 14:00:42 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_xl
SRCS=	if_xl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
