# $FreeBSD: src/sys/modules/uhci/Makefile,v 1.2.2.2.2.1 2010/02/10 00:26:20 kensmith Exp $

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	uhci
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c opt_usb.h
SRCS+=	uhci_pci.c uhci.c uhcireg.h uhcivar.h
SRCS+=	opt_bus.h pci_if.h

.include <bsd.kmod.mk>
