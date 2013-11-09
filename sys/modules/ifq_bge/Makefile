# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ifq_bge

KMOD=	ifq_bge
SRCS=	if_bge.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

.if ${MACHINE_CPUARCH} == sparc64
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
