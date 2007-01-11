# $FreeBSD: src/sys/modules/vr/Makefile,v 1.12 2001/01/06 14:00:40 obrien Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_vr
SRCS=	if_vr.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
