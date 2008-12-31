# $FreeBSD: src/sys/modules/ata/atapifd/Makefile,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapifd
SRCS=	atapi-fd.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
