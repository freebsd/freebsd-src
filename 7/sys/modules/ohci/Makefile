# $FreeBSD$

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	ohci
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c opt_usb.h
SRCS+=	ohci_pci.c ohci.c ohcireg.h ohcivar.h
SRCS+=	opt_bus.h pci_if.h

.include <bsd.kmod.mk>
