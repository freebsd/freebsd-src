# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

CFLAGS+=-g
KMOD=	atausb
SRCS=	ata-usb.c
SRCS+=  opt_usb.h opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
