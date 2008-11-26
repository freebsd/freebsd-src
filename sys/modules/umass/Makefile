# $FreeBSD: src/sys/modules/umass/Makefile,v 1.14.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c usbdevs.h

.include <bsd.kmod.mk>
