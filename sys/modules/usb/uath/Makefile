# $FreeBSD: src/sys/modules/usb/uath/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_uath
SRCS    = if_uath.c if_uathvar.h if_uathreg.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
