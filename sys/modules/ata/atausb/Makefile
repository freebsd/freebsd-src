# $FreeBSD: src/sys/modules/ata/atausb/Makefile,v 1.2 2006/03/11 08:42:08 sos Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atausb
SRCS=	ata-usb.c
SRCS+=  opt_usb.h opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
