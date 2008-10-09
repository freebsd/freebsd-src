# $FreeBSD$

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

.include <bsd.kmod.mk>
