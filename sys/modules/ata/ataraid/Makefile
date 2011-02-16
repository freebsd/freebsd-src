# $FreeBSD: src/sys/modules/ata/ataraid/Makefile,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ataraid
SRCS=	ata-raid.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
