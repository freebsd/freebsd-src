# $FreeBSD: src/sys/modules/ata/atausb/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atausb
SRCS=	ata-usb.c
SRCS+=  opt_usb.h opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
