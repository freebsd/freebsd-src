# $FreeBSD: src/sys/modules/hme/Makefile,v 1.3.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c miibus_if.h pci_if.h

.if ${MACHINE_ARCH} == "sparc64"
SRCS+=	if_hme_sbus.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
