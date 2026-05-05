S=     ${SRCTOP}/sys

.PATH: $S/dev/usb/video

KMOD=	uvideo
SRCS=	opt_bus.h opt_usb.h device_if.h \
	bus_if.h usb_if.h usbdevs.h \
	uvideo.c

.include <bsd.kmod.mk>
