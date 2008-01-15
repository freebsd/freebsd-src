# $FreeBSD: src/sys/modules/umass/Makefile,v 1.14 2005/06/04 10:58:38 schweikh Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c usbdevs.h

.include <bsd.kmod.mk>
