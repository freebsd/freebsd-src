# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_urtwn
SRCS    = if_urtwn.c if_urtwnreg.h \
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h opt_wlan.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
