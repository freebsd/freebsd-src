# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.9.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
