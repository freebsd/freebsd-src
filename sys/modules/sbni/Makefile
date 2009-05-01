# $FreeBSD: src/sys/modules/sbni/Makefile,v 1.1.36.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sbni

KMOD=	if_sbni
SRCS=	if_sbni.c if_sbni_isa.c if_sbni_pci.c

SRCS+=	bus_if.h device_if.h isa_if.h pci_if.h

.include <bsd.kmod.mk>
