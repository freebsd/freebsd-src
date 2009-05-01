# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.9.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
