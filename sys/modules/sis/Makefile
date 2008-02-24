# $FreeBSD: src/sys/modules/sis/Makefile,v 1.11 2005/09/27 18:10:38 mlaier Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_sis
SRCS=	if_sis.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
