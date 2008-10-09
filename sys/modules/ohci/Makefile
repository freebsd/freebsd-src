# $FreeBSD$

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	ohci
SRCS=	bus_if.h device_if.h \
	opt_usb.h \
	ohci_pci.c ohci.c ohcireg.h ohcivar.h \
	pci_if.h

.include <bsd.kmod.mk>
