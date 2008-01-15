# $FreeBSD: src/sys/modules/dc/Makefile,v 1.6 2001/01/06 13:59:44 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_dc
SRCS=	if_dc.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
