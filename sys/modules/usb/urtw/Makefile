# $FreeBSD: src/sys/modules/usb/urtw/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_urtw
SRCS    = if_urtw.c if_urtwreg.h if_urtwvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
