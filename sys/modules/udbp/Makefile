# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.9 2005/06/04 10:58:37 schweikh Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
