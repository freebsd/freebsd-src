# $FreeBSD: src/sys/modules/ata/ataraid/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ataraid
SRCS=	ata-raid.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
