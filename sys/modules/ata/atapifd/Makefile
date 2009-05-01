# $FreeBSD: src/sys/modules/ata/atapifd/Makefile,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapifd
SRCS=	atapi-fd.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
