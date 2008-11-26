# $FreeBSD: src/sys/modules/bge/Makefile,v 1.3.2.1.2.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bge

KMOD=	if_bge
SRCS=	opt_bge.h if_bge.c miibus_if.h miidevs.h opt_bdg.h device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == sparc64
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
