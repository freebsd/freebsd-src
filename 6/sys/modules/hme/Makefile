# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c miibus_if.h pci_if.h

.if ${MACHINE_ARCH} == "sparc64"
SRCS+=	if_hme_sbus.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
