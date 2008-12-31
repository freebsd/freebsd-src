# $FreeBSD: src/sys/modules/bge/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bge

KMOD=	if_bge
SRCS=	if_bge.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == sparc64
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
