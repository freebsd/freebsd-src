# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/bge

KMOD=	if_bge
SRCS=	opt_bge.h if_bge.c miibus_if.h miidevs.h opt_bdg.h device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == sparc64
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
