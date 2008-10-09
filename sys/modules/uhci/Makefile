# $FreeBSD$

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	uhci
SRCS=	bus_if.h device_if.h \
	opt_usb.h \
	uhci_pci.c uhci.c uhcireg.h uhcivar.h \
	pci_if.h

.include <bsd.kmod.mk>
