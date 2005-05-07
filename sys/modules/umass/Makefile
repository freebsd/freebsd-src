# $FreeBSD: src/sys/modules/umass/Makefile,v 1.13 2004/06/27 12:33:18 imp Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c usbdevs.h

.include <bsd.kmod.mk>
