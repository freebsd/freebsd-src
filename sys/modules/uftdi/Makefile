# $FreeBSD: src/sys/modules/uftdi/Makefile,v 1.4.22.1 2010/02/10 00:26:20 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uftdi
SRCS=	uftdi.c uftdireg.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
