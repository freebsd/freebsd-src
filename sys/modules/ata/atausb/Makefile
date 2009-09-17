# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atausb
SRCS=	ata-usb.c
SRCS+=	opt_bus.h opt_usb.h opt_ata.h usbdevs.h
SRCS+=	ata_if.h device_if.h bus_if.h pci_if.h usb_if.h

.include <bsd.kmod.mk>
