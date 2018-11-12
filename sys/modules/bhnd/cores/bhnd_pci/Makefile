# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/bhnd/cores/pci
.PATH: ${.CURDIR}/../../../../dev/bhnd/cores/pcie2

KMOD=	bhnd_pci
SRCS=	bhnd_pci.c bhnd_pcie2.c
SRCS+=	device_if.h bus_if.h bhnd_bus_if.h

.include <bsd.kmod.mk>
