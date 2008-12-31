# $FreeBSD: src/sys/modules/usb/Makefile,v 1.18.18.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..

.PATH: $S/dev/usb $S/pci

KMOD=	usb
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c \
	opt_usb.h \
	hid.c hid.h usbhid.h \
	uhub.c \
	usb.c usb.h \
	usb_mem.c usb_mem.h \
	usb_port.h \
	usb_quirks.c usb_quirks.h \
	usb_subr.c \
	usbdevs.h usbdevs_data.h \
	usbdi.c usbdi.h usbdivar.h \
	usbdi_util.c usbdi_util.h \
	usb_ethersubr.c usbdevs.h

SRCS+=	uhci_pci.c uhci.c uhcireg.h uhcivar.h
SRCS+=	ohci_pci.c ohci.c ohcireg.h ohcivar.h
SRCS+=	ehci_pci.c ehci.c ehcireg.h ehcivar.h
SRCS+=	opt_bus.h pci_if.h

.include <bsd.kmod.mk>
