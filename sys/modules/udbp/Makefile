# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.7 2004/06/27 12:33:17 imp Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
