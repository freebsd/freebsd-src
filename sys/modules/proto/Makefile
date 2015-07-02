# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/proto

KMOD=	proto
SRCS=	\
	proto_bus_isa.c \
	proto_bus_pci.c \
	proto_busdma.c \
	proto_core.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	isa_if.h \
	pci_if.h

MFILES=	\
	dev/pci/pci_if.m \
	isa/isa_if.m \
	kern/bus_if.m \
	kern/device_if.m

.include <bsd.kmod.mk>
