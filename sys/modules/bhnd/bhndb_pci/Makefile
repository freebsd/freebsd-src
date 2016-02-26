# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/bhnd/bhndb

KMOD=	bhndb_pci
SRCS=	bhndb_pci.c bhndb_pci_hwdata.c
SRCS+=	bhnd_bus_if.h bhndb_bus_if.h bhndb_if.h

SRCS+=	device_if.h bus_if.h

.include <bsd.kmod.mk>
