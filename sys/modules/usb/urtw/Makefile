# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_urtw
SRCS    = if_urtw.c if_urtwreg.h if_urtwvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
