S=     ${SRCTOP}/sys

.PATH: $S/dev/usb/serial

KMOD=	udbc
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h usbdevs.h \
	udbc.c

.include <bsd.kmod.mk>
