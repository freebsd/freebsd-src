# $FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/ntb/ntb_hw

KMOD    = ntb_hw
SRCS    = ntb_hw.c
SRCS += device_if.h bus_if.h pci_if.h ntb_if.h

MFILES=	kern/bus_if.m kern/device_if.m dev/pci/pci_if.m dev/ntb/ntb_if.m

.include <bsd.kmod.mk>
