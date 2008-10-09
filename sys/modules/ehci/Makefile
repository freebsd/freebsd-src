# $FreeBSD$

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	ehci
SRCS=	bus_if.h device_if.h \
	opt_usb.h \
	ehci_pci.c ehci.c ehcireg.h ehcivar.h \
	pci_if.h

.include <bsd.kmod.mk>
