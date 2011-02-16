# $FreeBSD: src/sys/modules/ohci/Makefile,v 1.2.2.2.4.1 2010/12/21 17:10:29 kensmith Exp $

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	ohci
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c opt_usb.h
SRCS+=	ohci_pci.c ohci.c ohcireg.h ohcivar.h
SRCS+=	opt_bus.h pci_if.h

.include <bsd.kmod.mk>
