# $FreeBSD: src/sys/modules/u3g/Makefile,v 1.3.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	u3g
SRCS=	opt_usb.h opt_u3g.h \
	device_if.h bus_if.h \
	usbdevs.h \
	ucomvar.h \
	u3g.c 

.include <bsd.kmod.mk>
