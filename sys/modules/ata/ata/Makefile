# $FreeBSD: src/sys/modules/ata/ata/Makefile,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ata
SRCS=	ata-all.c ata-queue.c ata-lowlevel.c ata_if.c
#	ata-pci.c ata-dma.c ata-chipset.c
SRCS+=	opt_ata.h ata_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
