# $FreeBSD: src/sys/modules/ata/atapci/chipsets/ataahci/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../../../dev/ata/chipsets

KMOD=	ataahci
SRCS=	ata-ahci.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

