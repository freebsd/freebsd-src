# $FreeBSD: src/sys/modules/ral/Makefile,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ral

KMOD=	if_ral
SRCS=	rt2560.c rt2661.c if_ral_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
