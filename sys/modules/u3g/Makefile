# $FreeBSD: src/sys/modules/u3g/Makefile,v 1.3.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	u3g
SRCS=	opt_usb.h opt_u3g.h \
	device_if.h bus_if.h \
	usbdevs.h \
	ucomvar.h \
	u3g.c 

.include <bsd.kmod.mk>
