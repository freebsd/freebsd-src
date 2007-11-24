# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapci
SRCS=	ata-pci.c ata-dma.c ata-chipset.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
