# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.9.24.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
