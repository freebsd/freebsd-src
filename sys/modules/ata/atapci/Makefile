# $FreeBSD: src/sys/modules/ata/atapci/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

SUBDIR += chipsets

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapci
SRCS=	ata-pci.c ata-dma.c ata-sata.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

