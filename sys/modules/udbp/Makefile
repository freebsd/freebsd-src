# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.9.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
