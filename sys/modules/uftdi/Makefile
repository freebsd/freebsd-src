# $FreeBSD: src/sys/modules/uftdi/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uftdi
SRCS=	uftdi.c uftdireg.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
