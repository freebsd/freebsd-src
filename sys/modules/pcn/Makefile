# $FreeBSD: src/sys/modules/pcn/Makefile,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_pcn
SRCS=	if_pcn.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
