S=	${SRCTOP}/sys

.PATH:	$S/dev/usb/wlan

KMOD=	if_mtw
SRCS=	opt_bus.h opt_usb.h opt_wlan.h device_if.h bus_if.h usb_if.h \
	usbdevs.h if_mtw.c

.include <bsd.kmod.mk>
