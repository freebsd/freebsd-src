# $FreeBSD: src/sys/modules/ral/Makefile,v 1.4.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ral

KMOD=	if_ral
SRCS=	rt2560.c rt2661.c if_ral_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
