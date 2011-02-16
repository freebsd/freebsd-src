# $FreeBSD: src/sys/modules/uftdi/Makefile,v 1.4.24.1 2010/12/21 17:10:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uftdi
SRCS=	uftdi.c uftdireg.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
